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
// Configuration Parameters
// a height of 5 gives 2⁵ (or 32) leaves. This number is small enough to keep the overall tree structure 
// manageable for testing while still illustrating the concept of random leaf assignments for obfuscation.
//
// a bucket capacity of 4 is a common academic starting point. It represents a trade-off: smaller buckets reduce
// the amount of data read per path but may increase the chance of stash overflows if not carefully managed.
//
//The stash acts as temporary storage for blocks read from the ORAM until they are evicted back. Setting it at 100 aims
// to minimize the risk of overflow during worst-case scenarios while keeping the simulation simple.
// -------------------------

constexpr int TREE_HEIGHT = 7;           // Height of the binary ORAM tree.
constexpr int BUCKET_CAPACITY = 8;       // Maximum number of blocks per bucket.
constexpr size_t STASH_LIMIT_DEFAULT = 500; // Maximum allowed blocks in the stash.

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
    std::vector<Block<K, V>> blocks;

    // Each bucket is initialized with a fixed number of dummy blocks.
    Bucket() {
        blocks.resize(BUCKET_CAPACITY);
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
    std::vector<Bucket<K, V>> tree;  // ORAM tree stored as a vector of buckets.
    int numBuckets;                  // Total number of buckets in the tree.
    int treeHeight;                   // Tree height.
    std::vector<Block<K, V>> stash;   // Temporary storage for accessed blocks.
    size_t stashLimit;                // Maximum allowed stash size.
    std::unordered_map<K, size_t> posMap; // Position map mapping keys to leaf nodes.
    mutable std::mutex mtx;           // Mutex for thread safety.

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
    // Constructor: Initializes ORAM tree and sets parameters.
    ObliviousMap(int height = TREE_HEIGHT, size_t stash_limit = STASH_LIMIT_DEFAULT)
        : treeHeight(height), stashLimit(stash_limit) {
        numBuckets = compute_numBuckets(treeHeight);
        tree.resize(numBuckets + 1); // Using 1-based indexing for the tree; index 0 remains unused. This simplifies arithmetic. 
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
};

#endif