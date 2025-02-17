
#include <iostream>
#include <unordered_map>
#include <string>

// ---------------------------------------------------------------------
// Structures for Interest and Data packets
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
// NDNRouter: Baseline NDN architecture with FIB, PIT, and CS.
// ---------------------------------------------------------------------
class NDNRouter {
private:
    // FIB: maps content name prefixes to forwarding interfaces.
    std::unordered_map<std::string, std::string> FIB;
    // PIT: maps content names to consumer identifiers for pending interests.
    std::unordered_map<std::string, std::string> PIT;
    // CS: simple cache that maps content names to content objects.
    std::unordered_map<std::string, Content> CS;

public:
    NDNRouter() {
        // populate the FIB
        FIB["/example"] = "eth0";
        FIB["/test"] = "eth1";
    }

    // Process interest packet
    void processInterest(const InterestPacket & interest) {
        std::cout << "\n[NDNRouter] Processing Interest for: " 
                  << interest.contentName << std::endl;
        // FIB lookup to determine the forwarding interface.
        auto fibIt = FIB.find(interest.contentName);
        if (fibIt != FIB.end()) {
            std::cout << "[NDNRouter] Forwarding interest on interface: " 
                      << fibIt->second << std::endl;
            // Update the PIT 
            PIT[interest.contentName] = interest.consumerId;
        } else {
            std::cout << "[NDNRouter] No matching FIB entry for: " 
                      << interest.contentName << std::endl;
        }
    }

    // Process data packet. Very similar to last function.
    void processData(const DataPacket & dataPkt) {
        std::cout << "\n[NDNRouter] Processing Data for: " 
                  << dataPkt.contentName << std::endl;
        // Store the data packet in CS
        Content content { dataPkt.contentName, dataPkt.data };
        CS[dataPkt.contentName] = content;
        std::cout << "[NDNRouter] Stored content: " << dataPkt.data 
                  << " in CS" << std::endl;

        // Check the PIT for a matching interest.
        auto pitIt = PIT.find(dataPkt.contentName);
        if (pitIt != PIT.end()) {
            std::cout << "[NDNRouter] Satisfying pending interest from consumer: " 
                      << pitIt->second << std::endl;
            // After serving the interest, remove the PIT entry. This is standard in NDN.
            PIT.erase(pitIt);
        } else {
            std::cout << "[NDNRouter] No pending interest found for content: " 
                      << dataPkt.contentName << std::endl;
        }
    }

    // Serve content directly from the Content Store.
    void serveContent(const std::string & contentName) {
        std::cout << "\n[NDNRouter] Serving content: " << contentName << std::endl;
        auto csIt = CS.find(contentName);
        if (csIt != CS.end()) {
            std::cout << "[NDNRouter] Content data: " << csIt->second.data << std::endl;
        } else {
            std::cout << "[NDNRouter] Content not found in CS." << std::endl;
        }
    }
};

// ---------------------------------------------------------------------
// Main simulation: Demonstrates baseline NDN operations.
// ---------------------------------------------------------------------
int main() {
    std::cout << "=== Baseline NDN Router Simulation ===" << std::endl;

    // Create an instance of the NDN router.
    NDNRouter router;

    // Simulate the arrival of an interest packet.
    InterestPacket interest;
    interest.contentName = "/example";
    interest.consumerId = "consumer1";
    router.processInterest(interest);

    // Simulate the arrival of a data packet corresponding to the interest.
    DataPacket dataPkt;
    dataPkt.contentName = "/example";
    dataPkt.data = "Hello, World!";
    router.processData(dataPkt);

    // Simulate a request to serve content from the Content Store.
    router.serveContent("/example");

    std::cout << "\n=== Simulation Complete ===" << std::endl;
    return 0;
}
