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
#include <mutex>              // For thread-safety

#include "crypto.hpp" // New crypto helper

// -------------------------
// Configuration Parameters
// -------------------------
constexpr int TREE_HEIGHT = 5;            // Height of the binary ORAM tree
constexpr int BUCKET_CAPACITY = 4;          // Maximum number of blocks per bucket
constexpr size_t STASH_LIMIT_DEFAULT = 100; // Maximum allowed blocks in the stash

// -------------------------
// Utility Functions
// -------------------------

// secure_random:
// Generates a cryptographically secure 32-bit random number.
inline uint32_t secure_random() {
    uint32_t num;
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&num), sizeof(num)) != 1) {
        throw std::runtime_error("RAND_bytes failed in secure_random");
    }
    return num;
}

// secure_random_index:
// Returns a random index in [0, range-1]; returns 0 if range is 0.
inline size_t secure_random_index(size_t range) {
    return (range == 0) ? 0 : secure_random() % range;
}

// -------------------------
// Block and Bucket Structures
// -------------------------

// Block:
// Represents a data block stored in the ORAM.
// Contains a key, an encrypted value, and a validity flag.
template<typename K, typename V>
struct Block {
    bool valid;  // True if this block contains valid data; false indicates a dummy block.
    K key;
    V value;     // The stored value (encrypted)

    // Default constructor creates an invalid (dummy) block.
    Block() : valid(false), key(), value() {}

    // Constructor for a real block.
    Block(const K& k, const V& v) : valid(true), key(k), value(v) {}
};

// Bucket:
// From my understanding, a bucket is a fixed-size container holding a vector of Blocks.
template<typename K, typename V>
struct Bucket {
    std::vector<Block<K,V>> blocks;
    
    // Bucket constructor initializes the bucket with BUCKET_CAPACITY dummy blocks.
    Bucket() {
        blocks.resize(BUCKET_CAPACITY);
    }
};

// -------------------------
// ObliviousMap Class (PathORAM-based)
// -------------------------
// Implements an oblivious map using a PathORAM structure.
// It provides oblivious insert and lookup operations by reading an entire path,
// using a stash to temporarily hold blocks, and evicting blocks back to the tree.
template<typename K, typename V>
class ObliviousMap {
private:
    std::vector<Bucket<K,V>> tree;         // The ORAM tree stored as a vector of buckets (1-indexed)
    int numBuckets;                        // Total number of buckets in the tree
    int treeHeight;                        // Height of the tree 
    std::vector<Block<K,V>> stash;         // The stash that temporarily holds blocks from accessed paths
    size_t stashLimit;                     // Maximum allowed stash size
    std::unordered_map<K, size_t> posMap;    // Client-side position map: maps keys to a random leaf index
    mutable std::mutex mtx;                // Mutex to protect concurrent accesses

    // compute_numBuckets:
    // Computes the total number of nodes (buckets) in a full binary tree of given height.
    int compute_numBuckets(int height) {
        return (1 << (height + 1)) - 1;
    }

    // get_path_indices:
    // Computes and returns the indices (in the tree vector) of the buckets along the path
    // from the root to the specified leaf (leaf index in [0, 2^treeHeight - 1]). I got this 
    // num from lectur.
    std::vector<int> get_path_indices(size_t leaf) {
        std::vector<int> path;
        // Leaves are stored starting at index (2^treeHeight, again from lecture)
        int index = (1 << treeHeight) - 1 + leaf;
        while (index > 0) {
            path.push_back(index);
            index /= 2;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    // read_path:
    // Reads all buckets along the given path and moves their valid blocks into the stash.
    // Throws an error if the stash exceeds its capacity.
    void read_path(const std::vector<int>& path) {
        for (int idx : path) {
            for (auto& blk : tree[idx].blocks) {
                if (blk.valid) {
                    stash.push_back(blk);
                    blk.valid = false; // Mark bucket slot as dummy after moving block to stash.
                }
            }
        }
        if (stash.size() > stashLimit) {
            throw std::runtime_error("Stash overflow error in read_path");
        }
    }

    // write_path (this function looks scary, but is fairly simple. just focus on comments):
   // Evicts eligible blocks from the stash back into the buckets along the accessed path.
    // This modified version repeatedly scans the path and evicts blocks until no more eligible
    // blocks can be placed, which helps prevent the stash from growing too large.
    void write_path(const std::vector<int>& path) {
        bool evictionPerformed = true;
        // Keep evicting while at least one block was moved in the last pass.
        while (evictionPerformed) {
            evictionPerformed = false;
            for (int idx : path) {
                Bucket<K,V>& bucket = tree[idx];
                // For each empty slot in the bucket...
                for (auto& slot : bucket.blocks) {
                    if (!slot.valid) { // empty slot found
                        // Search for an eligible block in the stash.
                        auto it = std::find_if(stash.begin(), stash.end(), [&](const Block<K,V>& blk) {
                            if (!blk.valid) return false; // Skip dummy blocks (should not occur in stash)
                            size_t blockLeaf = posMap.count(blk.key) ? posMap[blk.key] : 0;
                            std::vector<int> blockPath = get_path_indices(blockLeaf);
                            // If the current bucket index is on the block’s eligible path, it can be evicted here.
                            return std::find(blockPath.begin(), blockPath.end(), idx) != blockPath.end();
                        });
                        if (it != stash.end()) {
                            slot = *it;
                            stash.erase(it);
                            evictionPerformed = true;
                        }
                    }
                }
            }
        }
    }

    // remap_key:
    // Assigns a new random leaf to the key in the position map.
    // Returns the new leaf.
    size_t remap_key(const K& key) {
        size_t newLeaf = secure_random_index(1 << treeHeight);
        posMap[key] = newLeaf;
        return newLeaf;
    }

public:
    // Constructor:
    // Initializes the ORAM tree, sets the tree height and stash limit.
    ObliviousMap(int height = TREE_HEIGHT, size_t stash_limit = STASH_LIMIT_DEFAULT)
      : treeHeight(height), stashLimit(stash_limit)
    {
        numBuckets = compute_numBuckets(treeHeight);
        tree.resize(numBuckets + 1); // Use 1-based indexing; index 0 is unused.
    }

    // oblivious_insert:
    // Inserts a key-value pair into the oblivious map.
    // The value is encrypted using the crypto function before storage.
    // The operation performs a full-path read and then evicts blocks from the stash.
    void oblivious_insert(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mtx); // Lock for thread safety

        // Remap the key to a new random leaf.
        size_t leaf = remap_key(key);
        // Compute the path from the root to the assigned leaf.
        std::vector<int> path = get_path_indices(leaf);
        // Read the full path from the tree into the stash.
        read_path(path);
        // Encrypt the value using our enhanced crypto routine.
        stash.push_back(Block<K,V>(key, secure_encrypt_string(value)));
        if (stash.size() > stashLimit) {
            throw std::runtime_error("Stash overflow after insertion");
        }
        // Evict eligible blocks from the stash back into the tree.
        write_path(path);
    }

    // oblivious_lookup:
    // Searches for a key in the oblivious map.
    // If found, the encrypted value is decrypted (using enhanced decryption) and returned via the output parameter.
    // Also remaps the key (to hide repeated accesses).
    bool oblivious_lookup(const K& key, V& value) {
        std::lock_guard<std::mutex> lock(mtx); // Lock for thread safety

        // If key is not in the position map, it was never inserted.
        if (posMap.find(key) == posMap.end()) {
            return false;
        }
        size_t leaf = posMap[key];
        std::vector<int> path = get_path_indices(leaf);
        // Read the entire path into the stash.
        read_path(path);
        bool found = false;
        // Search the stash for the key.
        for (const auto& blk : stash) {
            if (blk.valid && blk.key == key) {
                // Decrypt the stored value using enhanced crypto.
                value = secure_decrypt_string(blk.value);
                found = true;
                break;
            }
        }
        // Remap the key to a new leaf to break linkability.
        if (found) {
            remap_key(key);
        }
        // Write back any eligible blocks from the stash.
        write_path(path);
        return found;
    }
};

#endif // TREE_MAP_HPP
