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
#include <thread>
#include <chrono>

#include "crypto.hpp"

// -------------------------
// Default Configuration Parameters
// -------------------------
constexpr int TREE_HEIGHT_DEFAULT = 8;          // Default height of the binary ORAM tree
constexpr int BUCKET_CAPACITY_DEFAULT = 12;        // Default maximum number of blocks per bucket
constexpr size_t STASH_LIMIT_DEFAULT = 100;       // Default maximum allowed blocks in the stash

// -------------------------
// Utility Functions
// -------------------------
inline uint32_t secure_random() {
    uint32_t num;
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&num), sizeof(num)) != 1) {
        throw std::runtime_error("RAND_bytes failed in secure_random");
    }
    return num;
}

inline size_t secure_random_index(size_t range) {
    return (range == 0) ? 0 : secure_random() % range;
}

// -------------------------
// Block and Bucket Structures
// -------------------------
template<typename K, typename V>
struct Block {
    bool valid;  
    K key;
    V value;
    size_t leaf; // assigned leaf index

    Block() : valid(false), key(), value(), leaf(0) {}
    Block(const K& k, const V& v, size_t leaf_) : valid(true), key(k), value(v), leaf(leaf_) {}
};

template<typename K, typename V>
struct Bucket {
    std::vector<Block<K,V>> blocks;
    Bucket(int capacity = BUCKET_CAPACITY_DEFAULT) {
        blocks.resize(capacity);
    }
};

// -------------------------
// ObliviousMap Class (PathORAM-based)
// -------------------------
template<typename K, typename V>
class ObliviousMap {
private:
    std::vector<Bucket<K,V>> tree;         // The ORAM tree (1-indexed)
    int numBuckets;                        
    int treeHeight;                        
    std::vector<Block<K,V>> stash;         
    size_t stashLimit;                     
    int bucketCapacity;                    
    std::unordered_map<K, size_t> posMap;  
    mutable std::mutex mtx;                

    // Background eviction thread components.
    std::atomic<bool> evictionThreadRunning;
    std::thread evictionThread;

    int compute_numBuckets(int height) {
        return (1 << (height + 1)) - 1;
    }

    // Returns the path from a leaf to the root.
    std::vector<int> get_path_indices(size_t leaf) {
        std::vector<int> path;
        int index = (1 << treeHeight) - 1 + leaf;
        while (index > 0) {
            path.push_back(index);
            index /= 2;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    // Reads blocks along the given path into the stash.
    void read_path(const std::vector<int>& path) {
        if (stash.size() > stashLimit * 0.75) {
            std::cerr << "[NDNRouter] WARNING: Stash approaching limit during read_path(). Running eviction.\n";
            full_eviction();
        }
        for (int idx : path) {
            for (auto& blk : tree[idx].blocks) {
                if (blk.valid) {
                    stash.push_back(blk);
                    blk.valid = false;
                }
            }
        }
        if (stash.size() > stashLimit) {
            throw std::runtime_error("Stash overflow error in read_path");
        }
    }

    // Eviction routine for blocks along a specific path.
    void write_path(const std::vector<int>& path) {
        while (stash.size() > stashLimit * 0.5) {
            size_t prevSize = stash.size();
            size_t evictedCount = 0;
            for (int bucketIndex : path) {
                Bucket<K, V>& bucket = tree[bucketIndex];
                for (auto &slot : bucket.blocks) {
                    if (!slot.valid) {
                        auto it = std::find_if(stash.begin(), stash.end(), [&](const Block<K,V>& blk) {
                            std::vector<int> blkPath = get_path_indices(blk.leaf);
                            return std::find(blkPath.begin(), blkPath.end(), bucketIndex) != blkPath.end();
                        });
                        if (it != stash.end()) {
                            slot = *it;
                            stash.erase(it);
                            evictedCount++;
                        }
                    }
                }
            }
            std::cerr << "[Eviction] write_path round: prev stash size = " << prevSize 
                      << ", evicted = " << evictedCount 
                      << ", new stash size = " << stash.size() << "\n";
            if (evictedCount == 0) {
                full_eviction();
            }
            if (stash.size() >= prevSize) {
                std::cerr << "[Eviction] write_path: no progress made, breaking loop\n";
                break;
            }
        }
    }
    
    // Full eviction scans the entire tree and evicts eligible blocks.
    // If no progress is made, reassign new leaf values to all blocks in the stash.
    void full_eviction() {
        while (stash.size() > stashLimit * 0.5) {
            size_t prevSize = stash.size();
            size_t evictedCount = 0;
            for (int idx = 1; idx <= numBuckets; idx++) {
                Bucket<K, V>& bucket = tree[idx];
                for (auto &slot : bucket.blocks) {
                    if (!slot.valid) {
                        auto it = std::find_if(stash.begin(), stash.end(), [&](const Block<K,V>& blk) {
                            std::vector<int> blkPath = get_path_indices(blk.leaf);
                            return std::find(blkPath.begin(), blkPath.end(), idx) != blkPath.end();
                        });
                        if (it != stash.end()) {
                            slot = *it;
                            stash.erase(it);
                            evictedCount++;
                        }
                    }
                }
            }
            std::cerr << "[Eviction] full_eviction round: prev stash size = " << prevSize 
                      << ", evicted = " << evictedCount 
                      << ", new stash size = " << stash.size() << "\n";
            if (evictedCount == 0 || stash.size() >= prevSize) {
                std::cerr << "[Eviction] full_eviction: no progress made, remapping stuck blocks\n";
                // Remap each block's leaf value in the stash.
                for (auto &blk : stash) {
                    blk.leaf = secure_random_index(1 << treeHeight);
                }
                // After remapping, attempt another eviction round.
                size_t postRemapSize = stash.size();
                if (postRemapSize >= prevSize) {
                    std::cerr << "[Eviction] full_eviction: still no progress after remapping, breaking loop\n";
                    break;
                }
            }
        }
    }

    // Remaps a key in the position map.
    size_t remap_key(const K& key) {
        size_t newLeaf = secure_random_index(1 << treeHeight);
        posMap[key] = newLeaf;
        return newLeaf;
    }

public:
    // Constructor: initializes the tree and starts a background eviction thread.
    ObliviousMap(int height = TREE_HEIGHT_DEFAULT, 
                 size_t stash_limit = STASH_LIMIT_DEFAULT,
                 int bucket_capacity = BUCKET_CAPACITY_DEFAULT)
      : treeHeight(height), stashLimit(stash_limit), bucketCapacity(bucket_capacity)
    {
        numBuckets = compute_numBuckets(treeHeight);
        tree.resize(numBuckets + 1); // 1-indexed tree.
        for (int i = 1; i <= numBuckets; i++) {
            tree[i] = Bucket<K,V>(bucketCapacity);
        }
        // Start background eviction thread.
        evictionThreadRunning = true;
        evictionThread = std::thread([this]() {
            while (evictionThreadRunning) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    if (stash.size() > stashLimit * 0.75) {
                        full_eviction();
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // Destructor: stops the background eviction thread.
    ~ObliviousMap() {
        evictionThreadRunning = false;
        if (evictionThread.joinable()) {
            evictionThread.join();
        }
    }

    void trigger_full_eviction() {
        std::lock_guard<std::mutex> lock(mtx);
        full_eviction();
    }

    // Inserts a key-value pair.
    void oblivious_insert(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mtx);
        size_t leaf = remap_key(key);
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);
        write_path(path);
        stash.push_back(Block<K, V>(key, secure_encrypt_string(value), leaf));
        write_path(path);
        full_eviction();
        if (stash.size() > stashLimit) {
            throw std::runtime_error("Stash overflow after insertion");
        }
    }

    // Looks up a key.
    bool oblivious_lookup(const K& key, V& value) {
        std::lock_guard<std::mutex> lock(mtx);
        if (posMap.find(key) == posMap.end()) return false;
        size_t leaf = posMap[key];
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);
        bool found = false;
        for (auto& blk : stash) {
            if (blk.valid && blk.key == key) {
                value = secure_decrypt_string(blk.value);
                blk.leaf = remap_key(key);
                found = true;
                break;
            }
        }
        write_path(path);
        full_eviction();
        return found;
    }
    
    size_t getStashSize() const {
        std::lock_guard<std::mutex> lock(mtx);
        return stash.size();
    }
    
    int getTreeHeight() const { return treeHeight; }
    int getBucketCapacity() const { return bucketCapacity; }
    size_t getStashLimit() const { return stashLimit; }
};

#endif
