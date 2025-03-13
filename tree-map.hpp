#ifndef TREE_MAP_HPP
#define TREE_MAP_HPP

#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <atomic>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cassert>
#include <mutex>

#include "crypto.hpp"

// -------------------------
// Default Configuration Parameters
// -------------------------
constexpr int TREE_HEIGHT_DEFAULT = 5;          // Default height of the binary ORAM tree
constexpr int BUCKET_CAPACITY_DEFAULT = 4;      // Default maximum number of blocks per bucket
constexpr size_t STASH_LIMIT_DEFAULT = 100;     // Default maximum allowed blocks in the stash

// -------------------------
// Utility Functions
// -------------------------

// Generates a cryptographically secure 32-bit random number.
inline uint32_t secure_random() {
    uint32_t num;
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&num), sizeof(num)) != 1) {
        throw std::runtime_error("RAND_bytes failed in secure_random");
    }
    return num;
}

// Returns a random index in [0, range-1]; returns 0 if range is 0.
inline size_t secure_random_index(size_t range) {
    return (range == 0) ? 0 : secure_random() % range;
}

// -------------------------
// Block and Bucket Structures
// -------------------------

// Block structure representing an ORAM data block.
// Contains a key, an encrypted value, and a validity flag.
template<typename K, typename V>
struct Block {
    bool valid;  // True if this block contains valid data.
    K key;
    V value;

    Block() : valid(false), key(), value() {}  // Default constructor for dummy
    Block(const K& k, const V& v) : valid(true), key(k), value(v) {}  // Constructor for valid data.
};

// Bucket containing multiple blocks.
template<typename K, typename V>
struct Bucket {
    std::vector<Block<K,V>> blocks;
    
    // Bucket constructor initializes the bucket with the specified capacity of dummy blocks.
    Bucket(int capacity = BUCKET_CAPACITY_DEFAULT) {
        blocks.resize(capacity);
    }
};

// -------------------------
// ObliviousMap Class (PathORAM-based)
// Implements an oblivious map using a PathORAM structure.
// Provides oblivious insert and lookup operations by reading an entire path,
// using a stash to temporarily hold blocks, and evicting blocks back to the tree.
// -------------------------

template<typename K, typename V>
class ObliviousMap {
private:
    std::vector<Bucket<K,V>> tree;         // The ORAM tree stored as a vector of buckets (1-indexed).
    int numBuckets;                        // Total number of buckets in the tree.
    int treeHeight;                        // Height of the tree.
    std::vector<Block<K,V>> stash;         // The stash temporarily holding blocks from accessed paths.
    size_t stashLimit;                     // Maximum allowed stash size.
    int bucketCapacity;                    // Bucket capacity (blocks per bucket)
    std::unordered_map<K, size_t> posMap;  // Client-side position map: maps keys to a random leaf index.
    mutable std::mutex mtx;                // Mutex to protect concurrent accesses.

    // Computes total number of buckets in a full binary tree of given height.
    int compute_numBuckets(int height) {
        return (1 << (height + 1)) - 1;
    }

    // Retrieves path indices from leaf to root (leaf index in [0, 2^treeHeight - 1])..
    // iterative to prevent stack overflows.
    std::vector<int> get_path_indices(size_t leaf) {
        std::vector<int> path;
        int index = (1 << treeHeight) - 1 + leaf;  // Convert leaf index to 1-based tree index.
        while (index > 0) {
            path.push_back(index);
            index /= 2;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    // Reads blocks along the accessed path into the stash.
    // Marks the bucket slots as dummy after moving.
    void read_path(const std::vector<int>& path) {
        for (int idx : path) {
            for (auto& blk : tree[idx].blocks) {
                if (blk.valid) {
                    stash.push_back(blk);
                    blk.valid = false;  // Mark slot as dummy.
                }
            }
        }
        if (stash.size() > stashLimit) {
            throw std::runtime_error("Stash overflow error in read_path");
        }
    }

    // Evicts blocks from stash back to tree buckets.
    // Added a limit on eviction attempts to prevent infinite loops.
    void write_path(const std::vector<int>& path) {
        const int MAX_EVICTION_ATTEMPTS = 10;
        int attempts = 0;
        bool evictionPerformed;

        do {
            evictionPerformed = false;
            for (int idx : path) {
                Bucket<K, V>& bucket = tree[idx];

                for (auto& slot : bucket.blocks) {
                    if (!slot.valid && !stash.empty()) {
                        slot = stash.back();  // Move block from stash.
                        stash.pop_back();
                        evictionPerformed = true;
                    }
                }
            }
            attempts++;
        } while (evictionPerformed && attempts < MAX_EVICTION_ATTEMPTS);
    }

    // Assigns a new random leaf to the key in position map.
    size_t remap_key(const K& key) {
        size_t newLeaf = secure_random_index(1 << treeHeight);
        posMap[key] = newLeaf;
        return newLeaf;
    }

public:
    // Constructor:
    // Initializes the ORAM tree, sets the tree height, bucket capacity, and stash limit.
    ObliviousMap(int height = TREE_HEIGHT_DEFAULT, 
                size_t stash_limit = STASH_LIMIT_DEFAULT,
                int bucket_capacity = BUCKET_CAPACITY_DEFAULT)
      : treeHeight(height), stashLimit(stash_limit), bucketCapacity(bucket_capacity)
    {
        numBuckets = compute_numBuckets(treeHeight);
        // Using 1-based indexing for the tree; index 0 remains unused. This simplifies arithmetic. 
        tree.resize(numBuckets + 1);
        
        // Initialize buckets with the specified capacity
        for (int i = 1; i <= numBuckets; i++) {
            tree[i] = Bucket<K,V>(bucketCapacity);
        }
    }

    /// oblivious_insert:
    // Inserts a key-value pair into the oblivious map.
    // Encrypts the value using the crypto function (AES-GCM) before storage.
    // Performs a full-path read and then evicts blocks from the stash.
    void oblivious_insert(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mtx);
        size_t leaf = remap_key(key);
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);

        stash.push_back(Block<K, V>(key, secure_encrypt_string(value)));

        if (stash.size() > stashLimit) {
            throw std::runtime_error("Stash overflow after insertion");
        }
        write_path(path);
    }

    // oblivious_lookup:
    // Searches for a key in the oblivious map.
    // If found, decrypts the value and returns it.
    // Also remaps the key to help break access linkability.
    bool oblivious_lookup(const K& key, V& value) {
        std::lock_guard<std::mutex> lock(mtx);
        if (posMap.find(key) == posMap.end()) return false;

        size_t leaf = posMap[key];
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);

        bool found = false;
        for (const auto& blk : stash) {
            if (blk.valid && blk.key == key) {
                value = secure_decrypt_string(blk.value);
                found = true;
                break;
            }
        }

        if (found) {
            remap_key(key);
        }
        write_path(path);
        return found;
    }
    
    // Get current stash size for metrics
    size_t getStashSize() const {
        std::lock_guard<std::mutex> lock(mtx);
        return stash.size();
    }
    
    // Get parameters for metrics
    int getTreeHeight() const { return treeHeight; }
    int getBucketCapacity() const { return bucketCapacity; }
    size_t getStashLimit() const { return stashLimit; }
};

#endif