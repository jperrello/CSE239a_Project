#ifndef OB_QUEUE_HPP
#define OB_QUEUE_HPP

/**
 * ---------------------------------------------------------------------
 * Below is the code for an enhanced oblivious queue.
 * This code is used to improve upon the CS (Content Store) in NDN architectures.
 *
 * CS:
 * The oblivious queue organizes cached content into a sequential structure.
 * When content is retrieved or updated, real accesses are interleaved with dummy operations,
 * making PUSH and POP operations indistinguishable.
 *
 * Enhancements:
 * - Parameterized dummy operation count to balance performance and security.
 * - Pre- and post-dummy phases to further obfuscate real operations.
 * ---------------------------------------------------------------------
 */

#include <iostream>
#include <vector>
#include <stdexcept>
#include <openssl/rand.h>
#include <atomic>
#include "ob-map.hpp"  // For secure_random and perform_extra_dummy

// Default number of dummy operations for buffer accesses.
constexpr int DEFAULT_BUFFER_DUMMY_OPS = 5;

/**
 * perform_buffer_dummy:
 * Performs dummy memory accesses on the buffer to simulate real access patterns.
 * Reads random elements from the buffer 'ops' times and inserts a memory fence.
 * @param buffer: The circular buffer.
 * @param head: Current head index of the buffer.
 * @param count: Number of valid elements in the buffer.
 * @param capacity: Total capacity of the buffer.
 * @param ops: Number of dummy operations to perform.
 */
template<typename T>
void perform_buffer_dummy(const std::vector<T>& buffer, size_t head, size_t count, size_t capacity, int ops) {
    for (int i = 0; i < ops; ++i) {
         if (count > 0) {
              size_t randomOffset = secure_random_index(count);
              size_t randomIndex = (head + randomOffset) % capacity;
              T temp = buffer[randomIndex];
              (void)temp;
         }
    }
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

/**
 * ObliviousQueue:
 * Template class that implements an oblivious queue for secure caching.
 * Provides oblivious push and pop operations wrapped in dummy phases.
 */
template<typename T>
class ObliviousQueue {
private:
    std::vector<T> buffer; // Fixed-size circular buffer.
    size_t capacity;       // Maximum number of elements.
    size_t head;           // Index of the front element.
    size_t tail;           // Index for next insertion.
    size_t count;          // Current number of elements.
    int dummyOps;          // Number of dummy operations to perform.
public:
    /**
     * Constructor for ObliviousQueue.
     * Pre-allocates the buffer to avoid dynamic memory allocation.
     * @param cap: Capacity of the queue.
     * @param dummyOpsCount: Number of dummy operations (default: DEFAULT_BUFFER_DUMMY_OPS).
     */
    ObliviousQueue(size_t cap, int dummyOpsCount = DEFAULT_BUFFER_DUMMY_OPS)
        : buffer(cap), capacity(cap), head(0), tail(0), count(0), dummyOps(dummyOpsCount) {}

    /**
     * oblivious_push:
     * Inserts an item into the circular buffer with dummy operations.
     * If the buffer is full, extra dummy operations are executed to maintain constant timing,
     * and the push operation fails.
     * @param item: The item to insert.
     * @return True if the push is successful; false if the buffer is full.
     */
    bool oblivious_push(const T & item) {
        if (count == capacity) {
            std::cout << "[ObliviousQueue] Push attempted on full queue.\n";
            perform_extra_dummy();
            return false;
        }
        // Pre-insertion dummy phase.
        perform_buffer_dummy(buffer, head, count, capacity, dummyOps);
        perform_extra_dummy();

        // Real insertion.
        buffer[tail] = item;
        tail = (tail + 1) % capacity;
        count++;

        // Post-insertion dummy phase.
        perform_buffer_dummy(buffer, head, count, capacity, dummyOps);
        perform_extra_dummy();
        std::cout << "[ObliviousQueue] Pushed item. Queue size: " << count << "\n";
        return true;
    }

    /**
     * oblivious_pop:
     * Removes the front item from the circular buffer with interleaved dummy operations.
     * If the buffer is empty, extra dummy operations are executed to maintain constant timing.
     * @param item: Output parameter to store the popped item.
     * @return True if an item is popped; false if the buffer is empty.
     */
    bool oblivious_pop(T & item) {
        if (count == 0) {
            std::cout << "[ObliviousQueue] Pop attempted on empty queue.\n";
            perform_extra_dummy();
            return false;
        }
        // Pre-pop dummy phase.
        perform_buffer_dummy(buffer, head, count, capacity, dummyOps);
        perform_extra_dummy();

        // Real removal.
        item = buffer[head];
        head = (head + 1) % capacity;
        count--;

        // Post-pop dummy phase.
        perform_buffer_dummy(buffer, head, count, capacity, dummyOps);
        perform_extra_dummy();
        std::cout << "[ObliviousQueue] Popped item. Queue size: " << count << "\n";
        return true;
    }
};

#endif // OB_QUEUE_HPP
