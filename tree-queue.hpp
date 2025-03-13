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

#include "crypto.hpp" 

// -------------------------
// Default Configuration Parameters
// -------------------------
constexpr int QUEUE_TREE_HEIGHT_DEFAULT = 4;       // Default height of the ORAM tree for the queue
constexpr int QUEUE_BUCKET_CAPACITY_DEFAULT = 8;   // Default maximum number of blocks per bucket
constexpr size_t QUEUE_STASH_LIMIT_DEFAULT = 100;  // Default maximum allowed stash size

// -------------------------
// QueueBlock and QueueBucket Structures
// -------------------------

// QueueBlock:
// Represents an item in the queue. The item is stored as an encrypted string.
template<typename T>
struct QueueBlock {
    bool valid;  // Indicates whether this block holds valid data.
    T data;      // The encrypted item.
    
    QueueBlock() : valid(false), data() {}
    QueueBlock(const T& d) : valid(true), data(d) {}
};

// QueueBucket:
// A bucket in the ORAM tree for the queue, consisting of a fixed number of QueueBlocks.
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
// Implements a queue using a PathORAM-based approach.
// The push and pop operations perform full-path accesses,
// using a stash to buffer items, then evicting them back into the tree.
template<typename T>
class ObliviousQueue {
private:
    std::vector<QueueBucket<T>> tree;      // The ORAM tree for the queue (1-indexed).
    int numBuckets;                        // Total number of buckets in the tree.
    int treeHeight;                        // Height of the tree.
    std::vector<QueueBlock<T>> stash;      // Stash to temporarily hold blocks.
    size_t stashLimit;                     // Maximum allowed stash size.
    int bucketCapacity;                    // Bucket capacity (blocks per bucket)
    mutable std::mutex mtx;                // Mutex for thread-safe operations.

    // compute_numBuckets:
    // Computes the total number of nodes in a full binary tree of given height.
    int compute_numBuckets(int height) {
        return (1 << (height + 1)) - 1;
    }

    // get_path_indices:
    // Returns the indices (in the tree) of buckets along the path from the root to the given leaf.
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

    // read_path:
    // Reads all valid blocks along the specified path into the stash.
    // Marks the corresponding blocks in the tree as invalid after reading.
    void read_path(const std::vector<int>& path) {
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

    // write_path:
    // Evicts items from the stash back into the buckets along the accessed path.
    // Uses a multi-pass FIFO strategy to flush the stash.
    void write_path(const std::vector<int>& path) {
        bool evictionPerformed = true;
        while(evictionPerformed && !stash.empty()) {
            evictionPerformed = false;
            for (int idx : path) {
                QueueBucket<T>& bucket = tree[idx];
                for (auto& slot : bucket.blocks) {
                    if (!slot.valid && !stash.empty()) {
                        slot = stash.front();
                        stash.erase(stash.begin());
                        evictionPerformed = true;
                    }
                }
            }
        }
    }

public:
    // Constructor:
    // Initializes the ORAM tree for the queue and sets the stash limit.
    ObliviousQueue(int height = QUEUE_TREE_HEIGHT_DEFAULT, 
                   size_t stash_limit = QUEUE_STASH_LIMIT_DEFAULT,
                   int bucket_capacity = QUEUE_BUCKET_CAPACITY_DEFAULT)
      : treeHeight(height), stashLimit(stash_limit), bucketCapacity(bucket_capacity)
    {
        numBuckets = compute_numBuckets(treeHeight);
        tree.resize(numBuckets + 1); // Use 1-based indexing.
        
        // Initialize buckets with the specified capacity
        for (int i = 1; i <= numBuckets; i++) {
            tree[i] = QueueBucket<T>(bucketCapacity);
        }
    }

    // oblivious_push:
    // Pushes an item into the queue.
    // Encrypts the item using the updated crypto function (AES-GCM) before enqueuing.
    void oblivious_push(const T& item) {
        std::lock_guard<std::mutex> lock(mtx); // Ensure thread safety.

        size_t leaf = secure_random_index(1 << treeHeight);
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);
        // Encrypt the item before pushing.
        stash.push_back(QueueBlock<T>(secure_encrypt_string(item)));
        if (stash.size() > stashLimit) {
            throw std::runtime_error("Stash overflow after push in queue");
        }
        write_path(path);
    }

    // oblivious_pop:
    // Pops an item from the queue.
    // If an item is found, it is decrypted and returned.
    // Returns true if an item was successfully popped; false otherwise.
    bool oblivious_pop(T& item) {
        std::lock_guard<std::mutex> lock(mtx); // Ensure thread safety.

        size_t leaf = secure_random_index(1 << treeHeight);
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);
        bool found = false;
        if (!stash.empty()) {
            QueueBlock<T> blk = stash.front();
            stash.erase(stash.begin());
            if (blk.valid) {
                item = secure_decrypt_string(blk.data);
                found = true;
            }
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
