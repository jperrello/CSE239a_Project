#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <iterator>

// Replace these with your header file includes:
#include "ob-map.hpp"
#include "ob-queue.hpp"

// ---------------------------------------------------------------------
// Structures for packets and content objects
// ---------------------------------------------------------------------
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

// ---------------------------------------------------------------------
// NDNRouter: Simulates an NDN router that integrates the oblivious 
// data structures for the FIB, PIT, and CS.
// ---------------------------------------------------------------------
class NDNRouter {
private:
    // FIB: maps content name prefixes to forwarding interfaces.
    ObliviousMap<std::string, std::string> FIB;
    // PIT: maps content names to consumer identifiers.
    ObliviousMap<std::string, std::string> PIT;
    // CS: stores content objects in an oblivious queue.
    ObliviousQueue<Content> CS;

public:
    NDNRouter() : CS(10) {  // Example: Set CS capacity to 10.
        // Pre-populate the FIB with example routes.
        FIB.oblivious_insert("/example", "eth0");
        FIB.oblivious_insert("/test", "eth1");
    }

    // Process an incoming interest packet.
    void handle_interest(const InterestPacket & interest) {
        std::cout << "\n[NDNRouter] Handling Interest Packet: " << interest.contentName << "\n";
        std::string outInterface;
        if (FIB.oblivious_lookup(interest.contentName, outInterface)) {
            std::cout << "[NDNRouter] Forwarding interest via interface: " << outInterface << "\n";
        } else {
            std::cout << "[NDNRouter] No matching FIB entry. Dropping interest.\n";
        }
        // Update PIT with the interest and consumer id.
        PIT.oblivious_insert(interest.contentName, interest.consumerId);
    }

    // Process an incoming data packet.
    void handle_data(const DataPacket & dataPacket) {
        std::cout << "\n[NDNRouter] Handling Data Packet: " << dataPacket.contentName << "\n";
        // Enqueue the content into the Content Store.
        Content content;
        content.name = dataPacket.contentName;
        content.data = dataPacket.data;
        CS.oblivious_push(content);
        
        // Check and remove the matching PIT entry.
        std::string consumer;
        if (PIT.oblivious_lookup(dataPacket.contentName, consumer)) {
            std::cout << "[NDNRouter] Found PIT entry for content: " << dataPacket.contentName
                      << " (consumer: " << consumer << ")\n";
            PIT.oblivious_remove(dataPacket.contentName);
        } else {
            std::cout << "[NDNRouter] No PIT entry found for content: " << dataPacket.contentName << "\n";
        }
    }

    // Serve content from the Content Store (CS) for a given content name.
    void serve_content(const std::string & contentName) {
        std::cout << "\n[NDNRouter] Attempting to serve content: " << contentName << "\n";
        Content content;
        if (CS.oblivious_pop(content)) {
            if (content.name == contentName) {
                std::cout << "[NDNRouter] Serving content data: " << content.data << "\n";
            } else {
                std::cout << "[NDNRouter] Popped content (" << content.name 
                          << ") does not match the requested content. Re-enqueueing.\n";
                // For simulation purposes, push the item back.
                CS.oblivious_push(content);
            }
        } else {
            std::cout << "[NDNRouter] Content Store is empty. Cannot serve content.\n";
        }
    }
};

int main() {
    NDNRouter router;
    // Create sample packets and simulate operations...
    InterestPacket interest{"/example", "consumer1"};
    router.handle_interest(interest);
    
    DataPacket dataPacket{"/example", "This is some content data."};
    router.handle_data(dataPacket);
    
    router.serve_content("/example");

    return 0;
}
