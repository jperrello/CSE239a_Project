/// ---------------------------------------------------------------------
// Below is the code for an oblivious queue.
// It will be used to improve upon the CS (Content Store) of NDN
// architectures. 
//
// ALSO: all the cout lines in this file should be removed if we actually
// want to implement this. it is logging all the info for debugging but 
// it would obviously leak info if we kept it.
//
// CS:
//  Builds on the idea behind Sparta’s oblivious multi queue but adapts it 
//  for caching in NDN. Instead of using an oblivious map, the CS is 
// structured as an oblivious queue. In practice, this means that cached 
// content is organized into a sequential structure where each entry (or node) 
// contains a content object and a pointer to the next entry. When content 
// is retrieved or updated, the operations interleave real accesses with 
// dummy operations using randomized access patterns, such that a PUSH and 
// POP protocol are indistinguishable. This method conceals the actual order 
// and number of cached items, thereby mitigating leakage of search patterns, 
// access patterns, and volume information, without simply padding every 
// operation to a worst-case scenario.
/// ---------------------------------------------------------------------
#include <iostream>
#include <vector>
#include <stdexcept>
#include <openssl/rand.h>
#include <atomic>   // For memory fences


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
/// perform_buffer_dummy:
///   Performs dummy memory accesses on the buffer to simulate real access patterns.
///   This function reads random elements from the buffer 'ops' times.
///   A memory fence is inserted afterward to prevent compiler reordering.
/// Template parameter T is the type of the elements in the buffer.
///--------------------------------------------------------------
template<typename T>
void perform_buffer_dummy(const std::vector<T>& buffer, size_t head, size_t count, size_t capacity, int ops) {
    for (int i = 0; i < ops; ++i) {
         size_t randomOffset = secure_random_index(count);
         size_t randomIndex = (head + randomOffset) % capacity;
         // Read a buffer element to simulate an access.
         T temp = buffer[randomIndex];
         (void)temp; // Prevent unused variable warnings.
    }
    // insert a compiler memory fence to prevent reordering of  dummy ops. 
    // aka ensure that the compiler does not optimize away these operations thus leaking its dummy.
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

///--------------------------------------------------------------
/// perform_extra_dummy:
///   Performs additional dummy computations to further obfuscate the operation pattern.
///   A memory fence is inserted afterward for same reasons as above.
///--------------------------------------------------------------
void perform_extra_dummy() {
    int accum = 0;
    const int extraOps = 10; //fixed dummy ops
    for (int i = 0; i < extraOps; ++i) {
         accum += i;
    }
    (void)accum;
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

template<typename T>
class ObliviousQueue {
private:
    std::vector<T> buffer; // Fixed-size circular buffer.
    size_t capacity;      
    size_t head;           
    size_t tail;           
    size_t count;          

public:
   
    /// The buffer is pre-allocated to avoid dynamic memory.
    ObliviousQueue(size_t cap)
        : buffer(cap), capacity(cap), head(0), tail(0), count(0) {}

    //--------------------------------------------------------------------------------------------------------------
    /// Oblivious Push: Insert an item into the circular buffer with dummy ops.
    /// This function operates in constant time and uses dummy ops to conceal the actual insertion.
    /// If the buffer is full, extra dummy ops are executed to maintain constant timing and the push fails.
    //--------------------------------------------------------------------------------------------------------------
    bool oblivious_push(const T & item) {
        if (count == capacity) {
            std::cout << "[ObliviousQueue] Push attempted on queue.\n";
            perform_extra_dummy();
            return false;
        }
        // Insert the item at the tail.
        buffer[tail] = item;
        tail = (tail + 1) % capacity;
        count++;

        // Perform dummy ops:
        // 1. Simulate buffer access with random reads.
        perform_buffer_dummy(buffer, head, count, capacity, 5);
        // 2. Execute extra dummy
        perform_extra_dummy();

        std::cout << "[ObliviousQueue] Pushed item. Queue size: " << count << "\n";
        return true;
    }

    //--------------------------------------------------------------------------------------------------------------
    /// Oblivious Pop: Remove the front item from the circular buffer with interleaved dummy operations.
    /// This function operates in constant time and uses dummy operations to conceal the actual removal.
    /// If the buffer is empty, extra dummy operations are executed to maintain consistent timing.
    //--------------------------------------------------------------------------------------------------------------
    bool oblivious_pop(T & item) {
        if (count == 0) {
            std::cout << "[ObliviousQueue] Pop attempted on empty queue.\n";
            perform_extra_dummy();
            return false;
        }
        // Perform dummy operations:
        // 1. Simulate buffer access with random reads.
        perform_buffer_dummy(buffer, head, count, capacity, 5);
        // 2. Execute extra dummy computations.
        perform_extra_dummy();

        // Retrieve the head.
        item = buffer[head];
        head = (head + 1) % capacity;
        count--;

        std::cout << "[ObliviousQueue] Popped item. Queue size: " << count << "\n";
        return true;
    }
};
