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
#include <mutex>              // For thread-safety

#include "crypto.hpp"

// -------------------------
// Configuration Parameters
// -------------------------
constexpr int QUEUE_TREE_HEIGHT = 4;            // Height of the ORAM tree for the queue
constexpr int QUEUE_BUCKET_CAPACITY = 8;          // Maximum number of blocks per bucket in the queue tree
constexpr size_t QUEUE_STASH_LIMIT_DEFAULT = 100; // Maximum allowed stash size for the queue

// -------------------------
// QueueBlock and QueueBucket Structures
// -------------------------

// QueueBlock:
// Represents an item in the queue.
// The item is stored as an encrypted string using enhanced crypto.
template<typename T>
struct QueueBlock {
    bool valid;  // Indicates whether this block holds a valid item.
    T data;      // The encrypted item.
    
    QueueBlock() : valid(false), data() {}
    QueueBlock(const T& d) : valid(true), data(d) {}
};

// QueueBucket:
// A bucket in the ORAM tree for the queue, consisting of a fixed number of QueueBlocks.
template<typename T>
struct QueueBucket {
    std::vector<QueueBlock<T>> blocks;
    QueueBucket() {
        blocks.resize(QUEUE_BUCKET_CAPACITY);
    }
};

// -------------------------
// ObliviousQueue Class (PathORAM-based for Queue)
// -------------------------

// ObliviousQueue:
// Implements a queue using a PathORAM-based approach.
// The push and pop operations perform full-path accesses,
// using a stash to buffer items and then evicting them back into the tree.
template<typename T>
class ObliviousQueue {
private:
    std::vector<QueueBucket<T>> tree;      // The ORAM tree for the queue (1-indexed)
    int numBuckets;                        // Total number of buckets in the tree
    int treeHeight;                        // Height of the tree
    std::vector<QueueBlock<T>> stash;      // Stash to temporarily hold blocks
    size_t stashLimit;                     // Maximum allowed stash size
    mutable std::mutex mtx;                // Mutex for thread-safe operations

    // compute_numBuckets:
    // Computes total nodes in a full binary tree of given height.
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
    // Throws an error if the stash size exceeds the limit.
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
    // For simplicity, FIFO order.
    // Evicts items from the stash back into the buckets along the accessed path.
// This version uses a  multi-pass strategy to flush the stash more aggressively.
    void write_path(const std::vector<int>& path) {
        bool evictionPerformed = true;
        // Keep evicting while there is progress and the stash is not empty.
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
    ObliviousQueue(int height = QUEUE_TREE_HEIGHT, size_t stash_limit = QUEUE_STASH_LIMIT_DEFAULT)
      : treeHeight(height), stashLimit(stash_limit)
    {
        numBuckets = compute_numBuckets(treeHeight);
        tree.resize(numBuckets + 1); // Use 1-based indexing.
    }

    // oblivious_push:
    // Pushes an item into the queue.
    // The item is encrypted using the crypto function and added to the stash,
    // then evicted back along a random path.
    void oblivious_push(const T& item) {
        std::lock_guard<std::mutex> lock(mtx); // Protect concurrent access

        size_t leaf = secure_random_index(1 << treeHeight);
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);
        // Encrypt the item before enqueuing.
        stash.push_back(QueueBlock<T>(secure_encrypt_string(item)));
        if (stash.size() > stashLimit) {
            throw std::runtime_error("Stash overflow after push in queue");
        }
        write_path(path);
    }

    // oblivious_pop:
    // Pops an item from the queue.
    // Returns true and sets the output parameter if an item is found; otherwise, returns false.
    bool oblivious_pop(T& item) {
        std::lock_guard<std::mutex> lock(mtx); // Protect concurrent access

        size_t leaf = secure_random_index(1 << treeHeight);
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);
        bool found = false;
        // FIFO
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
};

#endif 
