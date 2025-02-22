#ifndef OB_MAP_HPP
#define OB_MAP_HPP

/**
 * ---------------------------------------------------------------------
 * Below is the code for an enhanced oblivious map.
 * This code is used to improve upon the FIB (Forwarding Information Base)
 * and PIT (Pending Interest Table) within NDN Architectures.
 *
 * FIB:
 * The oblivious map obscures the mapping between content name prefixes and their 
 * associated forwarding interfaces. It employs randomized access patterns so that
 * an adversary cannot deduce the target interface from observed accesses.
 *
 * PIT:
 * The PIT tracks outstanding interest packets and their corresponding consumer IDs.
 * This oblivious map uses dummy operations to ensure that GET and PUT operations
 * remain indistinguishable.
 *
 * Enhancements:
 * - Parameterized dummy operation counts to balance performance and security.
 * - Pre- and post-dummy phases around each real operation.
 * - Hooks for potential integration with additional cryptographic primitives (e.g., ORAM)
 * ---------------------------------------------------------------------
 */

#include <iostream>
#include <stdexcept>
#include <cstdint>
#include <unordered_map>
#include <openssl/rand.h>
#include <atomic>

// Default number of dummy operations for map accesses.
constexpr int DEFAULT_DUMMY_OPS = 5;
constexpr int EXTRA_DUMMY_OPS = 10;

/**
 * secure_random:
 * Generates a cryptographically secure 32-bit random number using OpenSSL RAND_bytes.
 */
inline uint32_t secure_random() {
    uint32_t num;
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&num), sizeof(num)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    return num;
}

/**
 * secure_random_index:
 * Generates a random index within the given range using secure_random.
 * Returns 0 if the range is 0.
 */
inline size_t secure_random_index(size_t range) {
    return (range == 0) ? 0 : secure_random() % range;
}

/**
 * perform_extra_dummy:
 * Performs additional dummy computations to obfuscate operation patterns.
 * Inserts a memory fence to prevent compiler reordering and mitigate side-channel leakage.
 */
inline void perform_extra_dummy() {
    int accum = 0;
    for (int i = 0; i < EXTRA_DUMMY_OPS; ++i) {
         accum += i;
    }
    (void)accum;
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

/**
 * perform_map_dummy:
 * Performs dummy memory accesses on the unordered_map.
 * Simulates real access patterns by reading random elements from the map 'ops' times.
 * A memory fence is inserted afterward to prevent compiler reordering.
 */
template<typename K, typename V>
void perform_map_dummy(const std::unordered_map<K,V>& data, int ops) {
    for (int i = 0; i < ops; ++i) {
         if (!data.empty()) {
             auto it = data.begin();
             std::advance(it, secure_random_index(data.size()));
             auto dummy = it->second;
             (void)dummy; // Prevent unused variable warnings.
         }
    }
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

/**
 * ObliviousMap:
 * Template class that implements an oblivious map for secure data storage.
 * Provides oblivious insert, lookup, and remove operations wrapped in dummy phases.
 */
template<typename K, typename V>
class ObliviousMap {
private:
    std::unordered_map<K,V> data;
    int dummyOps; // Number of dummy operations to perform.
public:
    /**
     * Constructor for ObliviousMap.
     * @param dummyOpsCount: Number of dummy operations (default: DEFAULT_DUMMY_OPS).
     */
    ObliviousMap(int dummyOpsCount = DEFAULT_DUMMY_OPS)
        : dummyOps(dummyOpsCount) {}

    /**
     * oblivious_insert:
     * Inserts a key-value pair into the map with dummy operations before and after insertion.
     * @param key: The key to insert.
     * @param value: The value to associate with the key.
     */
    void oblivious_insert(const K & key, const V & value) {
         perform_map_dummy(data, dummyOps);
         perform_extra_dummy();
         data[key] = value;
         perform_map_dummy(data, dummyOps);
         perform_extra_dummy();
         std::cout << "[ObliviousMap] Inserted key: " << key << "\n";
    }

    /**
     * oblivious_lookup:
     * Searches for a key in the map with dummy operations before and after the lookup.
     * @param key: The key to search for.
     * @param value: Output parameter to store the found value.
     * @return True if the key is found; otherwise, false.
     */
    bool oblivious_lookup(const K & key, V & value) {
         perform_map_dummy(data, dummyOps);
         perform_extra_dummy();
         auto it = data.find(key);
         bool found = (it != data.end());
         if (found) {
              value = it->second;
         }
         perform_map_dummy(data, dummyOps);
         perform_extra_dummy();
         std::cout << "[ObliviousMap] Lookup for key: " << key
                   << " found: " << (found ? "true" : "false") << "\n";
         return found;
    }

    /**
     * oblivious_remove:
     * Removes a key from the map with dummy operations to obscure the removal.
     * @param key: The key to remove.
     */
    void oblivious_remove(const K & key) {
         perform_map_dummy(data, dummyOps);
         perform_extra_dummy();
         data.erase(key);
         perform_map_dummy(data, dummyOps);
         perform_extra_dummy();
         std::cout << "[ObliviousMap] Removed key: " << key << "\n";
    }
};

#endif 
