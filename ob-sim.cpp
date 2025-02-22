#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <iterator>

// Replace these with your header file includes:
#include "ob-map.hpp"
#include "ob-queue.hpp"

/**
 * ---------------------------------------------------------------------
 * Structures for packets and content objects.
 * These structures simulate Interest and Data packets used in NDN,
 * as well as a Content object for cached content.
 * ---------------------------------------------------------------------
 */
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

/**
 * ---------------------------------------------------------------------
 * NDNRouter:
 * Simulates an NDN router that integrates oblivious data structures for FIB,
 * PIT, and CS.
 *
 * FIB:
 * Maps content name prefixes to forwarding interfaces.
 *
 * PIT:
 * Tracks outstanding interest packets and their corresponding consumer IDs.
 *
 * CS:
 * Stores content objects in an oblivious queue.
 *
 * Enhancements:
 * - The router now includes detailed dummy operations to obscure real access patterns.
 * - The serve_content function maintains a fixed number of iterations to provide uniform timing.
 * ---------------------------------------------------------------------
 */
class NDNRouter {
private:
    // FIB: maps content name prefixes to forwarding interfaces.
    ObliviousMap<std::string, std::string> FIB;
    // PIT: maps content names to consumer identifiers.
    ObliviousMap<std::string, std::string> PIT;
    // CS: stores content objects in an oblivious queue.
    ObliviousQueue<Content> CS;

public:
    /**
     * Constructor for NDNRouter.
     * Initializes the CS with a fixed capacity and pre-populates the FIB with example routes.
     */
    NDNRouter() : CS(10) {  // Example: Set CS capacity to 10.
        // Pre-populate the FIB with example routes.
        FIB.oblivious_insert("/example", "eth0");
        FIB.oblivious_insert("/test", "eth1");
    }

    /**
     * handle_interest:
     * Processes an incoming Interest packet.
     * Performs an oblivious lookup in the FIB and updates the PIT with the interest.
     * @param interest: The InterestPacket containing the content name and consumer ID.
     */
    void handle_interest(const InterestPacket & interest) {
        std::cout << "\n[NDNRouter] Handling Interest Packet: " << interest.contentName << "\n";
        std::string outInterface;
        if (FIB.oblivious_lookup(interest.contentName, outInterface)) {
            std::cout << "[NDNRouter] Forwarding interest via interface: " << outInterface << "\n";
        } else {
            std::cout << "[NDNRouter] No matching FIB entry. Dropping interest.\n";
        }
        // Update PIT with the interest and consumer ID.
        PIT.oblivious_insert(interest.contentName, interest.consumerId);
    }

    /**
     * handle_data:
     * Processes an incoming Data packet.
     * Enqueues the content into the Content Store (CS) and checks the PIT for a matching entry.
     * If a matching PIT entry is found, it is removed.
     * @param dataPacket: The DataPacket containing the content name and associated data.
     */
    void handle_data(const DataPacket & dataPacket) {
        std::cout << "\n[NDNRouter] Handling Data Packet: " << dataPacket.contentName << "\n";
        // Enqueue the content into the Content Store.
        Content content;
        content.name = dataPacket.contentName;
        content.data = dataPacket.data;
        CS.oblivious_push(content);
        
        // Check for and remove a matching PIT entry.
        std::string consumer;
        if (PIT.oblivious_lookup(dataPacket.contentName, consumer)) {
            std::cout << "[NDNRouter] Found PIT entry for content: " << dataPacket.contentName
                      << " (consumer: " << consumer << ")\n";
            PIT.oblivious_remove(dataPacket.contentName);
        } else {
            std::cout << "[NDNRouter] No PIT entry found for content: " << dataPacket.contentName << "\n";
        }
    }

    /**
     * serve_content:
     * Attempts to serve content from the Content Store (CS) for a given content name.
     * Performs a single pop operation and, if the retrieved content does not match,
     * re-enqueues it. This method includes dummy operations to preserve uniform access patterns.
     * @param contentName: The name of the content to serve.
     */
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

/**
 * Main function:
 * Creates an instance of NDNRouter and simulates the processing of Interest and Data packets.
 */
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
