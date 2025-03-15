#ifndef TREE_QUEUE_HPP
#define TREE_QUEUE_HPP

#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <atomic>
#include <algorithm>
#include <string>
#include <cassert>
#include <mutex>
#include <thread>
#include <chrono>

#include "crypto.hpp"

// -------------------------
// Configuration Parameters
// -------------------------
constexpr int QUEUE_TREE_HEIGHT_DEFAULT = 8;       // Default tree height for the queue
constexpr int QUEUE_BUCKET_CAPACITY_DEFAULT = 12;     // Default maximum number of blocks per bucket
constexpr size_t QUEUE_STASH_LIMIT_DEFAULT = 100;    // Default maximum allowed stash size

// -------------------------
// QueueBlock and QueueBucket Structures
// -------------------------
template<typename T>
struct QueueBlock {
    bool valid;
    T data;
    size_t leaf; // Assigned leaf index

    QueueBlock() : valid(false), data(), leaf(0) {}
    QueueBlock(const T& d, size_t leaf_) : valid(true), data(d), leaf(leaf_) {}
};

template<typename T>
struct QueueBucket {
    std::vector<QueueBlock<T>> blocks;
    QueueBucket(int capacity = QUEUE_BUCKET_CAPACITY_DEFAULT) {
        blocks.resize(capacity);
    }
};

// -------------------------
// ObliviousQueue Class (PathORAM-based for Queue)
// -------------------------
template<typename T>
class ObliviousQueue {
private:
    std::vector<QueueBucket<T>> tree;      // The ORAM tree (1-indexed)
    int numBuckets;                        
    int treeHeight;                        
    std::vector<QueueBlock<T>> stash;      
    size_t stashLimit;                     
    int bucketCapacity;                    
    mutable std::mutex mtx;                

    // Background eviction thread components.
    std::atomic<bool> evictionThreadRunning;
    std::thread evictionThread;

    int compute_numBuckets(int height) {
        return (1 << (height + 1)) - 1;
    }

    // Returns the path from a given leaf to the root.
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

    // Reads blocks along the path into the stash.
    void read_path(const std::vector<int>& path) {
        if (stash.size() > stashLimit * 0.75) {
            std::cerr << "[ObliviousQueue] WARNING: Stash approaching limit. Running eviction.\n";
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
            throw std::runtime_error("Stash overflow in queue read_path");
        }
    }

    // Eviction routine along a specific path.
    void write_path(const std::vector<int>& path) {
        while (stash.size() > stashLimit * 0.5) {
            size_t prevSize = stash.size();
            size_t evictedCount = 0;
            for (int bucketIndex : path) {
                QueueBucket<T>& bucket = tree[bucketIndex];
                for (auto &slot : bucket.blocks) {
                    if (!slot.valid) {
                        auto it = std::find_if(stash.begin(), stash.end(), [&](const QueueBlock<T>& blk) {
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
    
    // Full eviction over the entire tree.
    // If no progress is made, reassign new random leaves to stuck blocks.
    void full_eviction() {
        while (stash.size() > stashLimit * 0.5) {
            size_t prevSize = stash.size();
            size_t evictedCount = 0;
            for (int idx = 1; idx <= numBuckets; idx++) {
                QueueBucket<T>& bucket = tree[idx];
                for (auto &slot : bucket.blocks) {
                    if (!slot.valid) {
                        auto it = std::find_if(stash.begin(), stash.end(), [&](const QueueBlock<T>& blk) {
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
                for (auto &blk : stash) {
                    blk.leaf = secure_random_index(1 << treeHeight);
                }
                if (stash.size() >= prevSize) {
                    std::cerr << "[Eviction] full_eviction: still no progress after remapping, breaking loop\n";
                    break;
                }
            }
        }
    }

public:
    // Constructor: initializes tree and starts background eviction.
    ObliviousQueue(int height = QUEUE_TREE_HEIGHT_DEFAULT, 
                   size_t stash_limit = QUEUE_STASH_LIMIT_DEFAULT,
                   int bucket_capacity = QUEUE_BUCKET_CAPACITY_DEFAULT)
      : treeHeight(height), stashLimit(stash_limit), bucketCapacity(bucket_capacity)
    {
        numBuckets = compute_numBuckets(treeHeight);
        tree.resize(numBuckets + 1);
        for (int i = 1; i <= numBuckets; i++) {
            tree[i] = QueueBucket<T>(bucketCapacity);
        }
        
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

    // Destructor: stops background eviction thread.
    ~ObliviousQueue() {
        evictionThreadRunning = false;
        if (evictionThread.joinable()) {
            evictionThread.join();
        }
    }

    void trigger_full_eviction() {
        std::lock_guard<std::mutex> lock(mtx);
        full_eviction();
    }

    // Pushes an item into the queue.
    void oblivious_push(const T& item) {
        std::lock_guard<std::mutex> lock(mtx);
        size_t leaf = secure_random_index(1 << treeHeight);
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);
        write_path(path);
        stash.push_back(QueueBlock<T>(secure_encrypt_string(item), leaf));
        full_eviction();
        if (stash.size() > stashLimit) {
            throw std::runtime_error("Stash overflow after push in queue");
        }
    }
    
    // Pops an item from the queue.
    bool oblivious_pop(T& item) {
        std::lock_guard<std::mutex> lock(mtx);
        size_t leaf = secure_random_index(1 << treeHeight);
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);
        bool found = false;
        if (!stash.empty()) {
            auto it = stash.begin();
            QueueBlock<T> blk = *it;
            stash.erase(it);
            if (blk.valid) {
                item = secure_decrypt_string(blk.data);
                found = true;
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
