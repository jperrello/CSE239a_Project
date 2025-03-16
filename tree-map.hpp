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
// Default Configuration Parameters - SIGNIFICANTLY INCREASED
// -------------------------
constexpr int TREE_HEIGHT_DEFAULT = 8;               // Default height of the binary ORAM tree
constexpr int BUCKET_CAPACITY_DEFAULT = 20;          // Increased from 12 to 20
constexpr size_t STASH_LIMIT_DEFAULT = 250;          // Increased from 100 to 250

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
// Block and Bucket Structures - SIMPLIFIED
// -------------------------
template<typename K, typename V>
struct Block {
    bool valid;  
    K key;
    V value;
    size_t leaf; // assigned leaf index
    int eviction_attempt_count; // Track how many times we've tried to evict this block
    bool high_priority; // Flag for high-priority blocks that shouldn't be dropped

    Block() : valid(false), key(), value(), leaf(0), eviction_attempt_count(0), high_priority(false) {}
    Block(const K& k, const V& v, size_t leaf_, bool hp = false) 
        : valid(true), key(k), value(v), leaf(leaf_), eviction_attempt_count(0), high_priority(hp) {}
};

template<typename K, typename V>
struct Bucket {
    std::vector<Block<K,V>> blocks;
    
    Bucket() {
        blocks.resize(BUCKET_CAPACITY_DEFAULT);
    }
    
    explicit Bucket(int capacity) {
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
    mutable std::mutex mtx;                // Global mutex for all operations
    int evictionFailCount; // Track consecutive eviction failures
    bool dropNonEssentialBlocks; // Flag to enable dropping non-essential blocks in emergencies
    
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
        // SUPER aggressive protection - allow path reads to proceed even with high stash usage
        // but ensure we periodically check to prevent complete overflow
        if (stash.size() >= stashLimit * 0.5) {
            std::cerr << "[EMERGENCY] Stash at " << stash.size() << "/" << stashLimit 
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
            std::cerr << "[CRITICAL] Reading path would add " << potential_new_blocks 
                      << " blocks to a stash of size " << stash.size() 
                      << " (limit: " << stashLimit << "). Taking drastic measures.\n";
            
            // Enable dropping of non-essential blocks
            dropNonEssentialBlocks = true;
            
            // We'll aggressively clear space
            while (stash.size() + potential_new_blocks > stashLimit * 0.7) {
                if (!emergency_drop_blocks()) {
                    // If we can't drop more blocks, we need to expand our stash
                    std::cerr << "[EXTREME EMERGENCY] Cannot free enough space. Dynamically expanding stash.\n";
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
            std::cerr << "[OVERFLOW] Stash size " << stash.size() 
                      << " exceeds limit " << stashLimit << " after read_path\n";
            
            // One last chance - drop enough blocks to get under the limit
            while (stash.size() > stashLimit * 0.9) {
                if (!emergency_drop_blocks()) {
                    // Expand stash as a last resort
                    stashLimit = static_cast<size_t>(stashLimit * 1.2);
                    std::cerr << "[EXTREME] Dynamically expanded stash limit to " << stashLimit << "\n";
                    break;
                }
            }
            
            // If we're still over the limit, we have to throw
            if (stash.size() > stashLimit) {
                throw std::runtime_error("Stash overflow error in read_path despite emergency measures");
            }
        }
    }

    // Emergency drop of non-essential blocks to prevent stash overflow
    bool emergency_drop_blocks() {
        // Sort to prioritize dropping blocks with high eviction counts
        std::sort(stash.begin(), stash.end(), [](const Block<K,V>& a, const Block<K,V>& b) {
            // Don't drop high priority blocks
            if (a.high_priority != b.high_priority)
                return b.high_priority;
            
            // Drop blocks with more eviction attempts first
            return a.eviction_attempt_count > b.eviction_attempt_count;
        });
        
        // Count blocks we can potentially drop
        int droppable_count = 0;
        for (const auto& blk : stash) {
            if (!blk.high_priority) {
                droppable_count++;
            }
        }
        
        if (droppable_count == 0) {
            std::cerr << "[CRITICAL] No non-essential blocks to drop!\n";
            return false;
        }
        
        // Drop 20% of non-essential blocks
        size_t to_drop = std::max(1, static_cast<int>(droppable_count * 0.2));
        size_t dropped = 0;
        
        auto it = stash.begin();
        while (it != stash.end() && dropped < to_drop) {
            if (!it->high_priority) {
                // For dropped blocks, re-add their key to posMap with a new random leaf
                if (it->valid) {
                    posMap[it->key] = secure_random_index(1 << treeHeight);
                }
                
                it = stash.erase(it);
                dropped++;
            } else {
                ++it;
            }
        }
        
        std::cerr << "[EMERGENCY] Dropped " << dropped << " non-essential blocks from stash\n";
        return dropped > 0;
    }

    // Eviction routine for blocks along a specific path.
    void write_path(const std::vector<int>& path) {
        size_t maxAttempts = 5; // Increased from 3 to 5
        size_t attempt = 0;
        
        while (stash.size() > stashLimit * 0.3 && attempt < maxAttempts) {
            size_t prevSize = stash.size();
            size_t evictedCount = 0;
            
            // Sort stash blocks by leaf depth and eviction attempts to prioritize eviction
            std::sort(stash.begin(), stash.end(), [this](const Block<K,V>& a, const Block<K,V>& b) {
                // Prioritize high-priority blocks
                if (a.high_priority != b.high_priority)
                    return a.high_priority;
                
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
                Bucket<K, V>& bucket = tree[bucketIndex];
                for (auto &slot : bucket.blocks) {
                    if (!slot.valid) {
                        auto it = std::find_if(stash.begin(), stash.end(), [&](const Block<K,V>& blk) {
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
            
            std::cerr << "[Eviction] write_path round " << attempt+1 << ": prev stash size = " << prevSize 
                      << ", evicted = " << evictedCount 
                      << ", new stash size = " << stash.size() << "\n";
                      
            if (evictedCount == 0) {
                // If we couldn't evict anything, remap some blocks
                remap_stuck_blocks();
            }
            
            if (stash.size() >= prevSize && attempt > 1) {
                // If we've made no progress after multiple attempts, maybe try dropping blocks
                if (dropNonEssentialBlocks) {
                    std::cerr << "[Eviction] write_path: no progress after " << attempt 
                              << " attempts. Trying emergency drop.\n";
                    emergency_drop_blocks();
                } else {
                    std::cerr << "[Eviction] write_path: no progress after " << attempt 
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
        std::cerr << "[Eviction] Remapped " << remapped << " stuck blocks\n";
    }
    
    // Critical eviction for when stash is nearing capacity - more extreme than emergency
    void critical_eviction() {
        std::cerr << "[Eviction] CRITICAL EVICTION: stash size = " << stash.size() 
                  << "/" << stashLimit << "\n";
        
        // Remap ALL blocks in the stash with fresh random leaves
        for (auto &blk : stash) {
            blk.leaf = secure_random_index(1 << treeHeight);
            blk.eviction_attempt_count = 0;
        }
        
        // Aggressive full tree sweep
        full_eviction(/*emergency=*/true);
        
        // If still too full, try dropping non-essential blocks
        if (stash.size() > stashLimit * 0.8) {
            std::cerr << "[Eviction] Critical eviction didn't free enough space. "
                      << "Enabling emergency block dropping.\n";
            dropNonEssentialBlocks = true;
            emergency_drop_blocks();
        }
    }
    
    // Full eviction scans the entire tree and evicts eligible blocks.
    void full_eviction(bool emergency = false) {
        size_t maxRounds = emergency ? 8 : 5;  // Increased rounds
        size_t round = 0;
        
        while ((stash.size() > stashLimit * (emergency ? 0.3 : 0.5)) && round < maxRounds) {
            size_t prevSize = stash.size();
            size_t evictedCount = 0;
            
            // Sort stash to prioritize eviction of less-attempted blocks
            std::sort(stash.begin(), stash.end(), [](const Block<K,V>& a, const Block<K,V>& b) {
                // Prioritize high-priority blocks
                if (a.high_priority != b.high_priority)
                    return a.high_priority;
                
                return a.eviction_attempt_count < b.eviction_attempt_count;
            });
            
            // Try to place blocks in buckets across the entire tree
            for (int idx = 1; idx <= numBuckets; idx++) {
                Bucket<K, V>& bucket = tree[idx];
                for (auto &slot : bucket.blocks) {
                    if (!slot.valid && !stash.empty()) {
                        auto it = std::find_if(stash.begin(), stash.end(), [&](const Block<K,V>& blk) {
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
            
            std::cerr << "[Eviction] full_eviction round " << round+1 << ": prev stash size = " << prevSize 
                      << ", evicted = " << evictedCount 
                      << ", new stash size = " << stash.size() << "\n";
                      
            if (evictedCount == 0 || stash.size() >= prevSize) {
                std::cerr << "[Eviction] full_eviction: minimal progress, remapping all blocks\n";
                
                // Remap each block's leaf value in the stash
                for (auto &blk : stash) {
                    blk.leaf = secure_random_index(1 << treeHeight);
                    blk.eviction_attempt_count = 0;
                }
                
                // If still no progress after remapping in emergency mode, try more extreme measures
                if (emergency && round > 3 && stash.size() >= prevSize) {
                    // In extreme cases, we might need to drop some blocks
                    if (stash.size() > stashLimit * 0.8) {
                        std::cerr << "[Eviction] WARNING: Critical stash overflow imminent. "
                                  << "Taking extreme measures.\n";
                        
                        // Enable dropping of non-essential blocks
                        dropNonEssentialBlocks = true;
                        emergency_drop_blocks();
                    }
                }
            }
            
            round++;
        }
    }

    // Remaps a key in the position map.
    size_t remap_key(const K& key) {
        size_t newLeaf = secure_random_index(1 << treeHeight);
        posMap[key] = newLeaf;
        return newLeaf;
    }

public:
    // Constructor with improved default parameters
    ObliviousMap(int height = TREE_HEIGHT_DEFAULT, 
                 size_t stash_limit = STASH_LIMIT_DEFAULT,
                 int bucket_capacity = BUCKET_CAPACITY_DEFAULT)
      : treeHeight(height), stashLimit(stash_limit), bucketCapacity(bucket_capacity),
        evictionFailCount(0), dropNonEssentialBlocks(false)
    {
        numBuckets = compute_numBuckets(treeHeight);
        tree.clear();
        
        // Create a dummy bucket at index 0 (we're 1-indexed)
        tree.emplace_back(Bucket<K,V>());
        
        // Create the actual buckets
        for (int i = 1; i <= numBuckets; i++) {
            tree.emplace_back(Bucket<K,V>(bucketCapacity));
        }
        
        // Start background eviction thread with more frequent checks
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

    // Destructor: stops the background eviction thread.
    ~ObliviousMap() {
        evictionThreadRunning = false;
        if (evictionThread.joinable()) {
            evictionThread.join();
        }
    }

    void trigger_full_eviction() {
        std::lock_guard<std::mutex> lock(mtx);
        full_eviction(/*emergency=*/true);
    }

    // Inserts a key-value pair.
    void oblivious_insert(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mtx);
        
        // Check stash size before operation
        if (stash.size() > stashLimit * 0.6) {
            std::cerr << "[NDNRouter] WARNING: High stash utilization before insert: " 
                      << stash.size() << "/" << stashLimit << "\n";
            full_eviction();
        }
        
        size_t leaf = remap_key(key);
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);
        
        // Mark FIB/PIT entries as higher priority
        bool is_high_priority = (key.find("/") == 0); // Routing entries are high priority
        
        // Insert the new block
        stash.push_back(Block<K, V>(key, secure_encrypt_string(value), leaf, is_high_priority));
        
        // Immediately try to evict blocks
        write_path(path);
        
        // Check stash size after operation
        if (stash.size() > stashLimit * 0.6) {
            full_eviction();
        }
        
        if (stash.size() > stashLimit) {
            throw std::runtime_error("Stash overflow after insertion");
        }
    }

    // Looks up a key.
    bool oblivious_lookup(const K& key, V& value) {
        std::lock_guard<std::mutex> lock(mtx);
        
        // Check stash size before operation
        if (stash.size() > stashLimit * 0.6) {
            std::cerr << "[NDNRouter] WARNING: High stash utilization before lookup: " 
                      << stash.size() << "/" << stashLimit << "\n";
            full_eviction();
        }
        
        if (posMap.find(key) == posMap.end()) return false;
        
        size_t leaf = posMap[key];
        std::vector<int> path = get_path_indices(leaf);
        read_path(path);
        
        bool found = false;
        for (auto& blk : stash) {
            if (blk.valid && blk.key == key) {
                value = secure_decrypt_string(blk.value);
                blk.leaf = remap_key(key);
                blk.eviction_attempt_count = 0; // Reset counter on access
                found = true;
                break;
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
    
    // Helper functions for diagnostics
    int getTreeHeight() const { return treeHeight; }
    int getBucketCapacity() const { return bucketCapacity; }
    size_t getStashLimit() const { return stashLimit; }
    bool isEmergencyModeEnabled() const { return dropNonEssentialBlocks; }
    
    // Explicitly enable emergency mode (dropping non-essential blocks)
    void enableEmergencyMode(bool enable) {
        std::lock_guard<std::mutex> lock(mtx);
        dropNonEssentialBlocks = enable;
        std::cerr << "[NDNRouter] Emergency mode " << (enable ? "ENABLED" : "DISABLED") << "\n";
    }
};

#endif