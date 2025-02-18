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
#include <openssl/rand.h>

#include <stdexcept>
#include <cstdint> // for fixed-width integer types 


///--------------------------------------------------------------
// Below are two functions. The first is a helper function that 
// use RAND_bytes to generate a cryptographically secure random 
// 32-bit number. Then, the second uses this to generate a random 
// index within a given range.
///--------------------------------------------------------------

// Returns a secure random 32-bit unsigned integer.
uint32_t secure_random() {
    uint32_t num;
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&num), sizeof(num)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    return num;
}

// Returns a secure random index within [0, range).
size_t secure_random_index(size_t range) {
    if (range == 0) return 0;
    return secure_random() % range;
}


///-------------------------------------------------------------------------
// Below is the code for the oblivious map itself. Takes in a key and a 
// value.
///-------------------------------------------------------------------------


template<typename K, typename V>
class ObliviousMap {
private:
    std::unordered_map<K,V> data;
    
public:
    ObliviousMap()

    ///--------------------------------------------------------------------------------------------------------------
    // Oblivious insert: Used to insert key-value pair with dummy access. 
    // Why dummy?
    // These dummy lookups are inserted to make the real operation (inserting a key) 
    // indistinguishable from other operations. In an environment where an adversary might monitor 
    // memory access patterns, these dummy operations help obfuscate which part of the code is doing the real work.
    // When choosing a number of dummy lookups, 5 is an arbitrary number. Remember there are 
    // trade offs for security and efficiency. Maybe there is a better where of picking dummy lookups. I haven't 
    // seen anything in research.
    ///--------------------------------------------------------------------------------------------------------------

    void oblivious_insert(const K & key, const V & value) {
        data[key] = value;
       
        const int dummyOps = 5;
        for (int i = 0; i < dummyOps; ++i) {
            if (!data.empty()) {
                auto it = data.begin();
                std::advance(it, secure_random_index(data.size()));
                volatile auto dummy = it->second;
                (void)dummy;
            }
        }
        std::cout << "[ObliviousMap] Inserted key: " << key << "\n";
    }

    ///--------------------------------------------------------------------------------------------------------------
    // Oblivious lookup: Used to Lookup key with dummy operations.
    // Dummy operations are performed before the actual lookup. This helps mask the timing and access patterns of the true lookup.
    // The call auto it = data.find(key); searches for the key in the underlying data structure.
    // If the key is found, we assign the value to the provided reference and set a flag to true.
    ///--------------------------------------------------------------------------------------------------------------

    bool oblivious_lookup(const K & key, V & value) {
        const int dummyOps = 5;
        for (int i = 0; i < dummyOps; ++i) {
            if (!data.empty()) {
                auto it = data.begin();
                std::advance(it, secure_random_index(data.size()));
                volatile auto dummy = it->second;
                (void)dummy;
            }
        }
        auto it = data.find(key);
        bool found = (it != data.end());
        if (found) {
            value = it->second;
        }
        std::cout << "[ObliviousMap] Lookup for key: " << key
                  << " found: " << (found ? "true" : "false") << "\n";
        return found;
    }

    ///--------------------------------------------------------------------------------------------------------------
    // Oblivious Remove: Remove key with dummy operations.
    // As in the other functions, we perform dummy operations to hide the actual removal. This is crucial because the 
    // timing of removals might reveal sensitive information.
    // data.erase(key); removes the specified key from the underlying unordered_map.
    ///--------------------------------------------------------------------------------------------------------------
    void oblivious_remove(const K & key) {
        const int dummyOps = 5;
        for (int i = 0; i < dummyOps; ++i) {
            if (!data.empty()){
                auto it = data.begin();
                std::advance(it, secure_random_index(data.size()));
                volatile auto dummy = it->second;
                (void)dummy;
            }
        }
        data.erase(key);
        std::cout << "[ObliviousMap] Removed key: " << key << "\n";
    }
};