#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <cassert>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "tree-map.hpp"
#include "tree-queue.hpp"

// -------------------------
// Structures for Packets and Content
// -------------------------
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

// -------------------------
// NDNRouter Class Using Oblivious Structures
// -------------------------
class NDNRouter {
private:
    ObliviousMap<std::string, std::string> FIB;
    ObliviousMap<std::string, std::string> PIT;
    ObliviousQueue<std::string> CS;
public:
    NDNRouter()
      : FIB(TREE_HEIGHT, 100), PIT(TREE_HEIGHT, 100), CS(QUEUE_TREE_HEIGHT, 100)
    {
        // Pre-populate the FIB with an example route.
        FIB.oblivious_insert("/example", "eth0");
    }

    void handle_interest(const InterestPacket& interest) {
        std::string outInterface;
        if (FIB.oblivious_lookup(interest.contentName, outInterface)) {
            std::cout << "[NDNRouter] Interest for \"" << interest.contentName
                      << "\" routed via " << outInterface << "\n";
        } else {
            std::cout << "[NDNRouter] No route for \"" << interest.contentName
                      << "\"; dropping interest.\n";
        }
        PIT.oblivious_insert(interest.contentName, interest.consumerId);
    }

    void handle_data(const DataPacket& dataPacket) {
        std::cout << "[NDNRouter] Handling data for \"" << dataPacket.contentName << "\"\n";
        std::string contentStr = dataPacket.contentName + ":" + dataPacket.data;
        CS.oblivious_push(contentStr);
        
        std::string consumer;
        if (PIT.oblivious_lookup(dataPacket.contentName, consumer)) {
            std::cout << "[NDNRouter] Found PIT entry for \"" << dataPacket.contentName
                      << "\" with consumer \"" << consumer << "\"\n";
            PIT.oblivious_insert(dataPacket.contentName, "dummy");
        } else {
            std::cout << "[NDNRouter] No PIT entry for \"" << dataPacket.contentName << "\"\n";
        }
    }

    bool serve_content(Content &servedContent) {
        std::string contentStr;
        if (CS.oblivious_pop(contentStr)) {
            size_t pos = contentStr.find(":");
            if (pos != std::string::npos) {
                servedContent.name = contentStr.substr(0, pos);
                servedContent.data = contentStr.substr(pos + 1);
                std::cout << "[NDNRouter] Serving content \"" << servedContent.name << "\"\n";
                return true;
            }
        }
        std::cout << "[NDNRouter] No content to serve.\n";
        return false;
    }
};

// -------------------------
// Parallel Test Harness
// -------------------------
void router_thread_func(NDNRouter& router, int thread_id) {
    try {
        InterestPacket interest{"/example", "consumer_" + std::to_string(thread_id)};
        router.handle_interest(interest);
        DataPacket data{"/example", "Content from thread " + std::to_string(thread_id)};
        router.handle_data(data);
        Content served;
        if (router.serve_content(served)) {
            std::cout << "[Thread " << thread_id << "] Served content: " 
                      << served.name << " -> " << served.data << "\n";
        } else {
            std::cout << "[Thread " << thread_id << "] No content served.\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "[Thread " << thread_id << "] Error: " << ex.what() << "\n";
    }
}

// -------------------------
// Unit Tests
// -------------------------
void run_unit_tests() {
    std::cout << "Running unit tests...\n";
    NDNRouter router;
    // Basic test: send an interest and check FIB lookup.
    InterestPacket interest{"/example", "unit_test_consumer"};
    router.handle_interest(interest);
    
    // Test PIT insertion and lookup.
    DataPacket data{"/example", "unit test data"};
    router.handle_data(data);
    Content served;
    assert(router.serve_content(served) && "Unit test failed: Content was not served as expected.");
    
    std::cout << "Unit tests completed successfully.\n";
}

// -------------------------
// Profiling Test
// -------------------------
void run_profiling_test() {
    std::cout << "Running profiling tests...\n";
    NDNRouter router;
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
         InterestPacket interest{"/example", "profiling"};
         router.handle_interest(interest);
         DataPacket data{"/example", "Data " + std::to_string(i)};
         router.handle_data(data);
         Content served;
         router.serve_content(served);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    std::cout << "Average time per iteration: " << (diff.count() / iterations) * 1000 << " ms\n";
}

// -------------------------
// Integration Test (Simulated Network Traffic)
// -------------------------
void run_integration_test() {
    std::cout << "Running integration test with simulated network traffic...\n";
    // Start a simple UDP server in a separate thread.
    const int port = 12345;
    std::thread server_thread([port]() {
         int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
         if (sockfd < 0) {
             std::cerr << "Server socket creation failed.\n";
             return;
         }
         sockaddr_in servaddr;
         memset(&servaddr, 0, sizeof(servaddr));
         servaddr.sin_family = AF_INET;
         servaddr.sin_addr.s_addr = INADDR_ANY;
         servaddr.sin_port = htons(port);
         if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
             std::cerr << "Bind failed on server.\n";
             close(sockfd);
             return;
         }
         char buffer[1024];
         sockaddr_in cliaddr;
         socklen_t len = sizeof(cliaddr);
         int n = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&cliaddr, &len);
         if (n < 0) {
             std::cerr << "Server receive failed.\n";
         } else {
             buffer[n] = '\0';
             std::cout << "Server received: " << buffer << "\n";
         }
         close(sockfd);
    });

    // Allow the server some time to start.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Client: send a test UDP packet.
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
         std::cerr << "Client socket creation failed.\n";
         return;
    }
    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    const char* msg = "NDN integration test interest packet";
    sendto(sockfd, msg, strlen(msg), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    close(sockfd);

    server_thread.join();
    std::cout << "Integration test completed.\n";
}

// -------------------------
// Main Function: Dispatch based on Command-line Argument
// -------------------------
int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string mode = argv[1];
        if (mode == "unittest") {
            run_unit_tests();
            return 0;
        } else if (mode == "profile") {
            run_profiling_test();
            return 0;
        } else if (mode == "integration") {
            run_integration_test();
            return 0;
        }
    }
    // Default: run the parallel test harness.
    try {
        NDNRouter router;
        const int num_threads = 4;
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(router_thread_func, std::ref(router), i+1);
        }
        for (auto& t : threads) {
            t.join();
        }
    } catch (const std::exception& ex) {
        std::cerr << "Main error: " << ex.what() << "\n";
    }
    return 0;
}
