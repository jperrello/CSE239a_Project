#include <gtest/gtest.h>
#include <string>
#include <chrono>
#include <thread>

// Include the headers for your oblivious data structures.
// (Make sure the include paths are set correctly in your build system.)
#include "ob-map.hpp"
#include "ob-queue.hpp"

// Optionally include the NDNRouter from ob-sim.cpp if refactored to be testable.
// For demonstration, we re-declare minimal structures for testing.
struct InterestPacket {
    std::string contentName;
    std::string consumerId;
};

struct DataPacket {
    std::string contentName;
    std::string data;
};

struct Content {
    std::string name;
    std::string data;
};

struct PITEntry {
    std::string consumerId;
    std::chrono::steady_clock::time_point timestamp;
};

constexpr std::chrono::seconds PIT_EXPIRATION_DURATION(5);

// A simplified version of the NDNRouter class for testing purposes.
class NDNRouter {
private:
    // FIB: maps content name prefixes to forwarding interfaces.
    ObliviousMap<std::string, std::string> FIB;
    // PIT: maps content names to PIT entries.
    ObliviousMap<std::string, PITEntry> PIT;
    // CS: stores content objects in an oblivious queue.
    ObliviousQueue<Content> CS;
    
    bool isPITEntryValid(const PITEntry &entry) {
        auto now = std::chrono::steady_clock::now();
        return (now - entry.timestamp) < PIT_EXPIRATION_DURATION;
    }
    
public:
    NDNRouter() : CS(10) {
        FIB.oblivious_insert("/example", "eth0");
        FIB.oblivious_insert("/test", "eth1");
    }

    // Process an incoming interest packet.
    void handle_interest(const InterestPacket & interest) {
        std::string outInterface;
        FIB.oblivious_lookup(interest.contentName, outInterface);
        PITEntry entry { interest.consumerId, std::chrono::steady_clock::now() };
        PIT.oblivious_insert(interest.contentName, entry);
    }

    // Process an incoming data packet.
    void handle_data(const DataPacket & dataPacket) {
        Content content { dataPacket.contentName, dataPacket.data };
        CS.oblivious_push(content);
        PITEntry pitEntry;
        if (PIT.oblivious_lookup(dataPacket.contentName, pitEntry) && isPITEntryValid(pitEntry)) {
            PIT.oblivious_remove(dataPacket.contentName);
        }
    }

    // Simplified serving: tries to pop content once.
    bool serve_content(const std::string & contentName, Content &servedContent) {
        return CS.oblivious_pop(servedContent);
    }
};

// -----------------------
// ObliviousMap Unit Tests
// -----------------------

TEST(ObliviousMapTest, InsertLookupRemove) {
    ObliviousMap<std::string, int> map;
    
    // Test insertion and lookup.
    map.oblivious_insert("key1", 10);
    int value = 0;
    bool found = map.oblivious_lookup("key1", value);
    EXPECT_TRUE(found);
    EXPECT_EQ(value, 10);
    
    // Test removal.
    map.oblivious_remove("key1");
    found = map.oblivious_lookup("key1", value);
    EXPECT_FALSE(found);
}

// -----------------------
// ObliviousQueue Unit Tests
// -----------------------

TEST(ObliviousQueueTest, PushPopAndOverflow) {
    // Create a queue with a small capacity.
    ObliviousQueue<int> queue(3);
    
    // Push elements until full.
    EXPECT_TRUE(queue.oblivious_push(1));
    EXPECT_TRUE(queue.oblivious_push(2));
    EXPECT_TRUE(queue.oblivious_push(3));
    // The queue should be full.
    EXPECT_FALSE(queue.oblivious_push(4));
    
    int val = 0;
    // Pop elements in order.
    EXPECT_TRUE(queue.oblivious_pop(val));
    EXPECT_EQ(val, 1);
    EXPECT_TRUE(queue.oblivious_push(4)); // Now there's space.
    EXPECT_TRUE(queue.oblivious_pop(val));
    EXPECT_EQ(val, 2);
    EXPECT_TRUE(queue.oblivious_pop(val));
    EXPECT_EQ(val, 3);
    EXPECT_TRUE(queue.oblivious_pop(val));
    EXPECT_EQ(val, 4);
    // Queue should be empty.
    EXPECT_FALSE(queue.oblivious_pop(val));
}

// -----------------------
// NDNRouter Integration Tests
// -----------------------

TEST(NDNRouterTest, HandleInterestAndData) {
    NDNRouter router;
    
    // Simulate receiving an interest.
    InterestPacket interest{"/example", "consumerTest"};
    router.handle_interest(interest);
    
    // Give a small delay to simulate network conditions.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Simulate receiving the corresponding data.
    DataPacket data {"/example", "Test content data"};
    router.handle_data(data);
    
    // Attempt to serve the content.
    Content servedContent;
    bool success = router.serve_content("/example", servedContent);
    
    // We expect that content was successfully popped from the CS.
    EXPECT_TRUE(success);
    // Although the oblivious queue might reorder dummy accesses,
    // in this simplified test we assume that the served content matches.
    EXPECT_EQ(servedContent.name, "/example");
    EXPECT_EQ(servedContent.data, "Test content data");
}

// -----------------------
// Main function for running all tests
// -----------------------

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
