/// ---------------------------------------------------------------------
// Below is the code for an oblivious map. 
// This code will be used to improve upon the FIB (Forwarding Information Base)
// and PIT (Personal Interest Table) within NDN Architectures.
//
// FIB:
// This oblivious map is employed to obscure the mapping between content name 
// prefixes and their associated forwarding interfaces.
// When an interest packet arrives, the router performs a randomized, 
// oblivious lookup in the FIB so that an adversary cannot determine which 
// interface is targeted based on the observed access pattern.
//
// PIT:
// The PIT is responsible for tracking outstanding interest packets and their 
// corresponding consumers. An oblivious map would be able to map content 
// names to consumer identifiers, and by using randomized access operations 
// with dummy queries, whether a GET or PUT protocol is being used will be 
// indistinguishable from one another. 
//
// ---------------------------------------------------------------------

// openSSL is being used to improve cryptographic security
// it will help us simulate dummy accesses in this file.
#include <iostream>
#include <stdexcept>
#include <cstdint>
#include <unordered_map>
#include <openssl/rand.h>
#include <atomic> 


///--------------------------------------------------------------
// Below are two functions. The first is a helper function that 
// use RAND_bytes to generate a cryptographically secure random 
// 32-bit number. Then, the second uses this to generate a random 
// index within a given range.
///--------------------------------------------------------------
uint32_t secure_random() {
    uint32_t num;
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&num), sizeof(num)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    return num;
}


size_t secure_random_index(size_t range) {
    if (range == 0) return 0;
    return secure_random() % range;
}

///--------------------------------------------------------------
// perform_extra_dummy: Performs additional dummy computations to 
// further obfuscate operation patterns.
// Inserts a memory barrier to prevent compiler reordering.
// AKA prevent side channel leakage.
///--------------------------------------------------------------
void perform_extra_dummy() {
    int accum = 0;
    const int extraOps = 10;  // Fixed num of extra dummy ops.
    for (int i = 0; i < extraOps; ++i) {
         accum += i;
    }
    (void)accum;
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

///--------------------------------------------------------------
// perform_map_dummy: Performs dummy memory accesses on the 
// unordered_map to simulate real access patterns.
// Reads random elements from the map 'ops' times and then inserts a memory fence.
///--------------------------------------------------------------
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


template<typename K, typename V>
class ObliviousMap {
private:
    std::unordered_map<K,V> data;
    
public:
    ObliviousMap() {}

    //--------------------------------------------------------------------------------------------------------------
    /// Oblivious Insert: Inserts a key-value pair with dummy operations to mask the real insertion.
    /// The dummy operations simulate random accesses and extra computation to hide memory access patterns.
    //--------------------------------------------------------------------------------------------------------------
    void oblivious_insert(const K & key, const V & value) {
         // Real insertion.
         data[key] = value;
         // Perform dummy ops to obfuscate the real access pattern.
         perform_map_dummy(data, 5);
         perform_extra_dummy();
         std::cout << "[ObliviousMap] Inserted key: " << key << "\n";
    }

    //--------------------------------------------------------------------------------------------------------------
    /// Oblivious Lookup: Searches for a key with dummy operations to mask the real lookup pattern.
    /// Dummy operations are executed before performing the actual lookup.
    //--------------------------------------------------------------------------------------------------------------
    bool oblivious_lookup(const K & key, V & value) {

         perform_map_dummy(data, 5); // fixed dummy ops
         perform_extra_dummy();
         // the actual lookup.
         auto it = data.find(key);
         bool found = (it != data.end());
         if (found) {
              value = it->second;
         }
         std::cout << "[ObliviousMap] Lookup for key: " << key
                   << " found: " << (found ? "true" : "false") << "\n";
         return found;
    }

    //--------------------------------------------------------------------------------------------------------------
    /// Oblivious Remove: Removes a key with dummy operations to hide the actual removal.
    /// The dummy operations are executed before performing the removal.
    //--------------------------------------------------------------------------------------------------------------
    void oblivious_remove(const K & key) {
         
         perform_map_dummy(data, 5);
         perform_extra_dummy();
         // actual removal.
         data.erase(key);
         std::cout << "[ObliviousMap] Removed key: " << key << "\n";
    }
};
