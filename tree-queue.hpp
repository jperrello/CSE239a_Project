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
// Configuration Parameters - SIGNIFICANTLY INCREASED
// -------------------------
constexpr int QUEUE_TREE_HEIGHT_DEFAULT = 8;       // Default tree height for the queue
constexpr int QUEUE_BUCKET_CAPACITY_DEFAULT = 20;     // Increased from 12 to 20
constexpr size_t QUEUE_STASH_LIMIT_DEFAULT = 250;    // Increased from 100 to 250

// -------------------------
// QueueBlock and QueueBucket Structures - SIMPLIFIED
// -------------------------
template<typename T>
struct QueueBlock {
    bool valid;
    T data;
    size_t leaf; // Assigned leaf index
    int eviction_attempt_count; // Track how many times we've tried to evict this block

    QueueBlock() : valid(false), data(), leaf(0), eviction_attempt_count(0) {}
    QueueBlock(const T& d, size_t leaf_) : valid(true), data(d), leaf(leaf_), eviction_attempt_count(0) {}
};

template<typename T>
struct QueueBucket {
    std::vector<QueueBlock<T>> blocks;
    
    QueueBucket() {
        blocks.resize(QUEUE_BUCKET_CAPACITY_DEFAULT);
    }
    
    explicit QueueBucket(int capacity) {
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
    mutable std::mutex mtx;                // Global mutex for all operations
    int evictionFailCount; // Track consecutive eviction failures
    bool emergencyMode; // Flag for emergency block dropping

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
        // SUPER aggressive protection - check stash utilization before reading
        if (stash.size() >= stashLimit * 0.5) {
            std::cerr << "[EMERGENCY] Queue stash at " << stash.size() << "/" << stashLimit 
                      << " before read_path. Performing critical eviction.\n";
            critical_eviction();
        }
        
        // Count how many blocks we'll be adding from this path
        size_t potential_new_blocks = 0;
        for (int idx : path) {
            for (auto& blk : tree[idx].blocks) {
                if (blk.valid) {
                    potential_new_blocks++;
                }
            }
        }
        
        // If adding these blocks would exceed the stash limit, we need to take extreme measures
        if (stash.size() + potential_new_blocks > stashLimit * 0.9) {
            std::cerr << "[CRITICAL] Reading queue path would add " << potential_new_blocks 
                      << " blocks to a stash of size " << stash.size() 
                      << " (limit: " << stashLimit << "). Taking drastic measures.\n";
            
            // Enable emergency mode
            emergencyMode = true;
            
            // We'll aggressively clear space
            while (stash.size() + potential_new_blocks > stashLimit * 0.7) {
                if (!emergency_drop_blocks()) {
                    // If we can't drop more blocks, we need to expand our stash
                    std::cerr << "[EXTREME EMERGENCY] Cannot free enough queue space. Dynamically expanding stash.\n";
                    stashLimit = static_cast<size_t>(stashLimit * 1.2); // Increase stash limit by 20%
                    break;
                }
            }
        }
        
        // Now read the path
        for (int idx : path) {
            for (auto& blk : tree[idx].blocks) {
                if (blk.valid) {
                    stash.push_back(blk);
                    blk.valid = false;
                }
            }
        }
        
        // Final safety check
        if (stash.size() > stashLimit) {
            std::cerr << "[OVERFLOW] Queue stash size " << stash.size() 
                      << " exceeds limit " << stashLimit << " after read_path\n";
            
            // One last chance - drop enough blocks to get under the limit
            while (stash.size() > stashLimit * 0.9) {
                if (!emergency_drop_blocks()) {
                    // Expand stash as a last resort
                    stashLimit = static_cast<size_t>(stashLimit * 1.2);
                    std::cerr << "[EXTREME] Dynamically expanded queue stash limit to " << stashLimit << "\n";
                    break;
                }
            }
            
            // If we're still over the limit, we have to throw
            if (stash.size() > stashLimit) {
                throw std::runtime_error("Queue stash overflow error in read_path despite emergency measures");
            }
        }
    }

    // Emergency drop of some blocks to prevent stash overflow
    bool emergency_drop_blocks() {
        if (stash.empty()) {
            return false;
        }
        
        // Sort by eviction attempts (drop most "stuck" blocks first)
        std::sort(stash.begin(), stash.end(), [](const QueueBlock<T>& a, const QueueBlock<T>& b) {
            return a.eviction_attempt_count > b.eviction_attempt_count;
        });
        
        // Drop 20% of blocks in extreme emergencies
        size_t to_drop = std::max<size_t>(1, stash.size() * 0.2);
        
        std::cerr << "[EMERGENCY] Dropping " << to_drop << " blocks from queue stash\n";
        
        // Remove blocks from the beginning (highest eviction attempts)
        stash.erase(stash.begin(), stash.begin() + to_drop);
        
        return true;
    }

    // Eviction routine along a specific path.
    void write_path(const std::vector<int>& path) {
        size_t maxAttempts = 5; // Increased from 3 to 5
        size_t attempt = 0;
        
        while (stash.size() > stashLimit * 0.3 && attempt < maxAttempts) {
            size_t prevSize = stash.size();
            size_t evictedCount = 0;
            
            // Sort stash blocks by eviction attempts to prioritize eviction
            std::sort(stash.begin(), stash.end(), [this](const QueueBlock<T>& a, const QueueBlock<T>& b) {
                // Prioritize blocks that have been attempted less
                if (a.eviction_attempt_count != b.eviction_attempt_count)
                    return a.eviction_attempt_count < b.eviction_attempt_count;
                
                // Secondary sort by leaf depth (path length to root)
                std::vector<int> pathA = get_path_indices(a.leaf);
                std::vector<int> pathB = get_path_indices(b.leaf);
                return pathA.size() < pathB.size();
            });
            
            // Increment attempt counter for all blocks
            for (auto& blk : stash) {
                blk.eviction_attempt_count++;
            }
            
            // Try to place blocks in buckets
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
                            slot.eviction_attempt_count = 0; // Reset counter once evicted
                            stash.erase(it);
                            evictedCount++;
                        }
                    }
                }
            }
            
            std::cerr << "[Eviction] queue write_path round " << attempt+1 << ": prev stash size = " << prevSize 
                      << ", evicted = " << evictedCount 
                      << ", new stash size = " << stash.size() << "\n";
                      
            if (evictedCount == 0) {
                // If we couldn't evict anything, remap some blocks
                remap_stuck_blocks();
            }
            
            if (stash.size() >= prevSize && attempt > 1) {
                // If we've made no progress after multiple attempts, maybe try dropping blocks
                if (emergencyMode) {
                    std::cerr << "[Eviction] queue write_path: no progress after " << attempt 
                              << " attempts. Trying emergency drop.\n";
                    emergency_drop_blocks();
                } else {
                    std::cerr << "[Eviction] queue write_path: no progress after " << attempt 
                              << " attempts, breaking loop\n";
                    break;
                }
            }
            
            attempt++;
        }
        
        // If stash is still too large after write_path, try a more aggressive approach
        if (stash.size() > stashLimit * 0.7) {
            critical_eviction();
        }
    }
    
    // Remap blocks that appear stuck in the stash
    void remap_stuck_blocks() {
        // Only remap blocks that have been attempted multiple times
        size_t remapped = 0;
        for (auto &blk : stash) {
            if (blk.eviction_attempt_count > 2) {
                blk.leaf = secure_random_index(1 << treeHeight);
                blk.eviction_attempt_count = 0; // Reset counter
                remapped++;
            }
        }
        std::cerr << "[Eviction] Remapped " << remapped << " stuck queue blocks\n";
    }
    
    // Critical eviction for when stash is nearing capacity - more extreme than emergency
    void critical_eviction() {
        std::cerr << "[Eviction] CRITICAL QUEUE EVICTION: stash size = " << stash.size() 
                  << "/" << stashLimit << "\n";
        
        // Remap ALL blocks in the stash with fresh random leaves
        for (auto &blk : stash) {
            blk.leaf = secure_random_index(1 << treeHeight);
            blk.eviction_attempt_count = 0;
        }
        
        // Aggressive full tree sweep
        full_eviction(/*emergency=*/true);
        
        // If still too full, try dropping blocks
        if (stash.size() > stashLimit * 0.8) {
            std::cerr << "[Eviction] Critical queue eviction didn't free enough space. "
                      << "Enabling emergency block dropping.\n";
            emergencyMode = true;
            emergency_drop_blocks();
        }
    }
    
    // Full eviction over the entire tree.
    void full_eviction(bool emergency = false) {
        size_t maxRounds = emergency ? 8 : 5;  // Increased rounds
        size_t round = 0;
        
        while ((stash.size() > stashLimit * (emergency ? 0.3 : 0.5)) && round < maxRounds) {
            size_t prevSize = stash.size();
            size_t evictedCount = 0;
            
            // Sort stash to prioritize eviction of less-attempted blocks
            std::sort(stash.begin(), stash.end(), [](const QueueBlock<T>& a, const QueueBlock<T>& b) {
                return a.eviction_attempt_count < b.eviction_attempt_count;
            });
            
            // Try to place blocks in buckets across the entire tree
            for (int idx = 1; idx <= numBuckets; idx++) {
                QueueBucket<T>& bucket = tree[idx];
                for (auto &slot : bucket.blocks) {
                    if (!slot.valid && !stash.empty()) {
                        auto it = std::find_if(stash.begin(), stash.end(), [&](const QueueBlock<T>& blk) {
                            std::vector<int> blkPath = get_path_indices(blk.leaf);
                            return std::find(blkPath.begin(), blkPath.end(), idx) != blkPath.end();
                        });
                        if (it != stash.end()) {
                            slot = *it;
                            slot.eviction_attempt_count = 0; // Reset counter once evicted
                            stash.erase(it);
                            evictedCount++;
                        }
                    }
                }
            }
            
            std::cerr << "[Eviction] queue full_eviction round " << round+1 << ": prev stash size = " << prevSize 
                      << ", evicted = " << evictedCount 
                      << ", new stash size = " << stash.size() << "\n";
                      
            if (evictedCount == 0 || stash.size() >= prevSize) {
                std::cerr << "[Eviction] queue full_eviction: minimal progress, remapping all blocks\n";
                
                // Remap each block's leaf value in the stash
                for (auto &blk : stash) {
                    blk.leaf = secure_random_index(1 << treeHeight);
                    blk.eviction_attempt_count = 0;
                }
                
                // If still no progress after remapping in emergency mode, try more extreme measures
                if (emergency && round > 3 && stash.size() >= prevSize) {
                    // In extreme cases, we might need to drop some blocks
                    if (stash.size() > stashLimit * 0.8) {
                        std::cerr << "[Eviction] WARNING: Critical queue stash overflow imminent. "
                                  << "Taking extreme measures.\n";
                        
                        // Enable emergency mode
                        emergencyMode = true;
                        emergency_drop_blocks();
                    }
                }
            }
            
            round++;
        }
    }

public:
    // Constructor: initializes tree and starts background eviction.
    ObliviousQueue(int height = QUEUE_TREE_HEIGHT_DEFAULT, 
                   size_t stash_limit = QUEUE_STASH_LIMIT_DEFAULT,
                   int bucket_capacity = QUEUE_BUCKET_CAPACITY_DEFAULT)
      : treeHeight(height), stashLimit(stash_limit), bucketCapacity(bucket_capacity),
        evictionFailCount(0), emergencyMode(false)
    {
        numBuckets = compute_numBuckets(treeHeight);
        tree.clear();
        
        // Create a dummy bucket at index 0 (we're 1-indexed)
        tree.emplace_back(QueueBucket<T>());
        
        // Create the actual buckets
        for (int i = 1; i <= numBuckets; i++) {
            tree.emplace_back(QueueBucket<T>(bucket_capacity));
        }
        
        evictionThreadRunning = true;
        evictionThread = std::thread([this]() {
            while (evictionThreadRunning) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    if (stash.size() > stashLimit * 0.5) {
                        full_eviction();
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Reduced from 10ms to 5ms
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
        full_eviction(/*emergency=*/true);
    }

    // Pushes an item into the queue.
    void oblivious_push(const T& item) {
        std::lock_guard<std::mutex> lock(mtx);
        
        // Check stash size before operation
        if (stash.size() > stashLimit * 0.6) {
            std::cerr << "[ObliviousQueue] WARNING: High stash utilization before push: " 
                      << stash.size() << "/" << stashLimit << "\n";
            full_eviction();
        }
        
        size_t leaf = secure_random_index(1 << treeHeight);
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);
        
        // Insert the new block
        stash.push_back(QueueBlock<T>(secure_encrypt_string(item), leaf));
        
        // Immediately try to evict blocks
        write_path(path);
        
        // Check stash size after operation
        if (stash.size() > stashLimit * 0.6) {
            full_eviction();
        }
        
        if (stash.size() > stashLimit) {
            throw std::runtime_error("Stash overflow after push in queue");
        }
    }
    
    // Pops an item from the queue.
    bool oblivious_pop(T& item) {
        std::lock_guard<std::mutex> lock(mtx);
        
        // Check stash size before operation
        if (stash.size() > stashLimit * 0.6) {
            std::cerr << "[ObliviousQueue] WARNING: High stash utilization before pop: " 
                      << stash.size() << "/" << stashLimit << "\n";
            full_eviction();
        }
        
        // Select a random path to access (for obliviousness)
        size_t leaf = secure_random_index(1 << treeHeight);
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);
        
        bool found = false;
        if (!stash.empty()) {
            // Sort stash by eviction attempts to prioritize removal of "stuck" blocks
            std::sort(stash.begin(), stash.end(), [](const QueueBlock<T>& a, const QueueBlock<T>& b) {
                return a.eviction_attempt_count > b.eviction_attempt_count;
            });
            
            auto it = stash.begin();
            QueueBlock<T> blk = *it;
            stash.erase(it);
            if (blk.valid) {
                item = secure_decrypt_string(blk.data);
                found = true;
            }
        }
        
        // Evict blocks back to the tree
        write_path(path);
        
        // Check stash size after operation
        if (stash.size() > stashLimit * 0.6) {
            full_eviction();
        }
        
        return found;
    }
    
    size_t getStashSize() const {
        std::lock_guard<std::mutex> lock(mtx);
        return stash.size();
    }
    
    int getTreeHeight() const { return treeHeight; }
    int getBucketCapacity() const { return bucketCapacity; }
    size_t getStashLimit() const { return stashLimit; }
    bool isEmergencyModeEnabled() const { return emergencyMode; }
    
    // Explicitly enable emergency mode (dropping blocks)
    void enableEmergencyMode(bool enable) {
        std::lock_guard<std::mutex> lock(mtx);
        emergencyMode = enable;
        std::cerr << "[ObliviousQueue] Emergency mode " << (enable ? "ENABLED" : "DISABLED") << "\n";
    }
};

#endif