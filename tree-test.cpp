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
#include <fstream>
#include <random>
#include <algorithm>
#include <numeric>
#include <iomanip>

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
// Performance Metrics Structure
// -------------------------
struct PerformanceMetrics {
    // Throughput
    int totalOperations = 0;
    double totalTimeSeconds = 0;
    
    // Latency (microseconds)
    std::vector<double> interestLatencies;
    std::vector<double> dataLatencies;
    std::vector<double> retrievalLatencies;
    
    // Memory usage
    size_t peakMemoryUsage = 0;
    
    // Stash metrics
    size_t maxStashSize = 0;
    std::vector<size_t> stashSizeHistory;
    
    void clear() {
        totalOperations = 0;
        totalTimeSeconds = 0;
        interestLatencies.clear();
        dataLatencies.clear();
        retrievalLatencies.clear();
        peakMemoryUsage = 0;
        maxStashSize = 0;
        stashSizeHistory.clear();
    }
    
    void printSummary(const std::string& title) const {
        std::cout << "\n===== " << title << " =====\n";
        std::cout << "Total operations: " << totalOperations << "\n";
        std::cout << "Total time: " << totalTimeSeconds << " seconds\n";
        std::cout << "Throughput: " << (totalOperations / totalTimeSeconds) << " ops/sec\n";
        
        // Calculate latency statistics
        auto calcStats = [](const std::vector<double>& latencies) {
            if (latencies.empty()) return std::make_tuple(0.0, 0.0, 0.0);
            double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
            double mean = sum / latencies.size();
            
            std::vector<double> sorted = latencies;
            std::sort(sorted.begin(), sorted.end());
            double median = sorted[sorted.size() / 2];
            
            double variance = 0.0;
            for (double x : latencies) {
                variance += (x - mean) * (x - mean);
            }
            variance /= latencies.size();
            double stddev = std::sqrt(variance);
            
            return std::make_tuple(mean, median, stddev);
        };
        
        auto [interestMean, interestMedian, interestStdDev] = calcStats(interestLatencies);
        auto [dataMean, dataMedian, dataStdDev] = calcStats(dataLatencies);
        auto [retrievalMean, retrievalMedian, retrievalStdDev] = calcStats(retrievalLatencies);
        
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Interest handling latency (μs): mean=" << interestMean 
                  << ", median=" << interestMedian << ", stddev=" << interestStdDev << "\n";
        std::cout << "Data handling latency (μs): mean=" << dataMean 
                  << ", median=" << dataMedian << ", stddev=" << dataStdDev << "\n";
        std::cout << "Content retrieval latency (μs): mean=" << retrievalMean 
                  << ", median=" << retrievalMedian << ", stddev=" << retrievalStdDev << "\n";
        
        if (!stashSizeHistory.empty()) {
            double avgStash = std::accumulate(stashSizeHistory.begin(), stashSizeHistory.end(), 0.0) / stashSizeHistory.size();
            std::cout << "Max stash size: " << maxStashSize << "\n";
            std::cout << "Average stash size: " << avgStash << "\n";
        }
        
        std::cout << "Peak memory usage: " << (peakMemoryUsage / 1024.0 / 1024.0) << " MB\n";
    }
    
    void saveToCSV(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filename << "\n";
            return;
        }
        
        // Write header
        file << "Metric,Value\n";
        file << "TotalOperations," << totalOperations << "\n";
        file << "TotalTimeSeconds," << totalTimeSeconds << "\n";
        file << "Throughput," << (totalOperations / totalTimeSeconds) << "\n";
        
        // Calculate statistics
        auto calcStats = [](const std::vector<double>& latencies) {
            if (latencies.empty()) return std::make_tuple(0.0, 0.0, 0.0);
            double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
            double mean = sum / latencies.size();
            
            std::vector<double> sorted = latencies;
            std::sort(sorted.begin(), sorted.end());
            double median = sorted[sorted.size() / 2];
            
            double variance = 0.0;
            for (double x : latencies) {
                variance += (x - mean) * (x - mean);
            }
            variance /= latencies.size();
            double stddev = std::sqrt(variance);
            
            return std::make_tuple(mean, median, stddev);
        };
        
        auto [interestMean, interestMedian, interestStdDev] = calcStats(interestLatencies);
        auto [dataMean, dataMedian, dataStdDev] = calcStats(dataLatencies);
        auto [retrievalMean, retrievalMedian, retrievalStdDev] = calcStats(retrievalLatencies);
        
        file << "InterestLatencyMean," << interestMean << "\n";
        file << "InterestLatencyMedian," << interestMedian << "\n";
        file << "InterestLatencyStdDev," << interestStdDev << "\n";
        file << "DataLatencyMean," << dataMean << "\n";
        file << "DataLatencyMedian," << dataMedian << "\n";
        file << "DataLatencyStdDev," << dataStdDev << "\n";
        file << "RetrievalLatencyMean," << retrievalMean << "\n";
        file << "RetrievalLatencyMedian," << retrievalMedian << "\n";
        file << "RetrievalLatencyStdDev," << retrievalStdDev << "\n";
        
        if (!stashSizeHistory.empty()) {
            double avgStash = std::accumulate(stashSizeHistory.begin(), stashSizeHistory.end(), 0.0) / stashSizeHistory.size();
            file << "MaxStashSize," << maxStashSize << "\n";
            file << "AvgStashSize," << avgStash << "\n";
        }
        
        file << "PeakMemoryUsageMB," << (peakMemoryUsage / 1024.0 / 1024.0) << "\n";
        
        // Write raw data for further analysis
        file << "\nRaw Interest Latencies (μs)\n";
        for (double lat : interestLatencies) {
            file << lat << "\n";
        }
        
        file << "\nRaw Data Latencies (μs)\n";
        for (double lat : dataLatencies) {
            file << lat << "\n";
        }
        
        file << "\nRaw Retrieval Latencies (μs)\n";
        for (double lat : retrievalLatencies) {
            file << lat << "\n";
        }
        
        file << "\nStash Size History\n";
        for (size_t size : stashSizeHistory) {
            file << size << "\n";
        }
        
        file.close();
        std::cout << "Performance data saved to " << filename << "\n";
    }
};

// Function to estimate current memory usage
size_t getCurrentMemoryUsage() {
    // This is a platform-specific implementation
    // On Linux, you could read from /proc/self/statm
    std::ifstream statm("/proc/self/statm");
    if (statm.is_open()) {
        size_t size, resident, share, text, lib, data, dt;
        statm >> size >> resident;
        // Convert to bytes (multiply by page size, typically 4KB)
        return resident * 4096;
    }
    return 0; // Fallback if not available
}

// -------------------------
// NDNRouter Class with Performance Tracking
// -------------------------
class NDNRouter {
private:
    ObliviousMap<std::string, std::string> FIB;
    ObliviousMap<std::string, std::string> PIT;
    ObliviousQueue<std::string> CS;
    PerformanceMetrics metrics;

public:
NDNRouter(bool collectMetrics = false, int treeHeight = TREE_HEIGHT, 
    int queueHeight = QUEUE_TREE_HEIGHT, size_t stashSize = 500) : FIB(treeHeight, stashSize), PIT(treeHeight, stashSize), CS(queueHeight, stashSize)
    {
        // Pre-populate the FIB with example routes
        FIB.oblivious_insert("/example", "eth0");
        FIB.oblivious_insert("/content", "eth1");
        FIB.oblivious_insert("/videos", "eth2");
        
        if (collectMetrics) {
            metrics.clear();
        }
    }

    void handle_interest(const InterestPacket& interest) {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::string outInterface;
        if (FIB.oblivious_lookup(interest.contentName, outInterface)) {
            std::cout << "[NDNRouter] Interest for \"" << interest.contentName
                      << "\" routed via " << outInterface << "\n";
        } else {
            std::cout << "[NDNRouter] No route for \"" << interest.contentName
                      << "\"; dropping interest.\n";
        }
        PIT.oblivious_insert(interest.contentName, interest.consumerId);
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> diff = end - start;
        metrics.interestLatencies.push_back(diff.count());
        metrics.totalOperations++;
        
        // Update stash metrics if access is available
        // Note: You might need to modify ObliviousMap to expose stash size
        // metrics.stashSizeHistory.push_back(FIB.getStashSize());
        // metrics.maxStashSize = std::max(metrics.maxStashSize, FIB.getStashSize());
        
        // Update memory usage
        size_t currentMemory = getCurrentMemoryUsage();
        metrics.peakMemoryUsage = std::max(metrics.peakMemoryUsage, currentMemory);
    }

    void handle_data(const DataPacket& dataPacket) {
        auto start = std::chrono::high_resolution_clock::now();
        
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
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> diff = end - start;
        metrics.dataLatencies.push_back(diff.count());
        metrics.totalOperations++;
        
        // Update memory usage
        size_t currentMemory = getCurrentMemoryUsage();
        metrics.peakMemoryUsage = std::max(metrics.peakMemoryUsage, currentMemory);
    }

    bool serve_content(Content &servedContent) {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::string contentStr;
        bool success = false;
        if (CS.oblivious_pop(contentStr)) {
            size_t pos = contentStr.find(":");
            if (pos != std::string::npos) {
                servedContent.name = contentStr.substr(0, pos);
                servedContent.data = contentStr.substr(pos + 1);
                std::cout << "[NDNRouter] Serving content \"" << servedContent.name << "\"\n";
                success = true;
            }
        } else {
            std::cout << "[NDNRouter] No content to serve.\n";
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> diff = end - start;
        metrics.retrievalLatencies.push_back(diff.count());
        metrics.totalOperations++;
        
        // Update memory usage
        size_t currentMemory = getCurrentMemoryUsage();
        metrics.peakMemoryUsage = std::max(metrics.peakMemoryUsage, currentMemory);
        
        return success;
    }
    
    const PerformanceMetrics& getMetrics() const {
        return metrics;
    }
    
    void resetMetrics() {
        metrics.clear();
    }
    
    void startMetricCollection() {
        metrics.clear();
    }
    
    void stopMetricCollection(double elapsedTimeSeconds) {
        metrics.totalTimeSeconds = elapsedTimeSeconds;
    }
};

// -------------------------
// Baseline NDN Router (No Privacy Measures)
// -------------------------
class BaselineNDNRouter {
private:
    std::unordered_map<std::string, std::string> FIB;
    std::unordered_map<std::string, std::string> PIT;
    std::vector<std::string> CS;
    PerformanceMetrics metrics;

public:
    BaselineNDNRouter(bool collectMetrics = false) {
        // Pre-populate the FIB with example routes (same as privacy version)
        FIB["/example"] = "eth0";
        FIB["/content"] = "eth1";
        FIB["/videos"] = "eth2";
        
        if (collectMetrics) {
            metrics.clear();
        }
    }

    void handle_interest(const InterestPacket& interest) {
        auto start = std::chrono::high_resolution_clock::now();
        
        if (FIB.find(interest.contentName) != FIB.end()) {
            std::cout << "[BaselineNDN] Interest for \"" << interest.contentName
                      << "\" routed via " << FIB[interest.contentName] << "\n";
        } else {
            std::cout << "[BaselineNDN] No route for \"" << interest.contentName
                      << "\"; dropping interest.\n";
        }
        PIT[interest.contentName] = interest.consumerId;
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> diff = end - start;
        metrics.interestLatencies.push_back(diff.count());
        metrics.totalOperations++;
        
        // Update memory usage
        size_t currentMemory = getCurrentMemoryUsage();
        metrics.peakMemoryUsage = std::max(metrics.peakMemoryUsage, currentMemory);
    }

    void handle_data(const DataPacket& dataPacket) {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::cout << "[BaselineNDN] Handling data for \"" << dataPacket.contentName << "\"\n";
        std::string contentStr = dataPacket.contentName + ":" + dataPacket.data;
        CS.push_back(contentStr);
        
        if (PIT.find(dataPacket.contentName) != PIT.end()) {
            std::cout << "[BaselineNDN] Found PIT entry for \"" << dataPacket.contentName
                      << "\" with consumer \"" << PIT[dataPacket.contentName] << "\"\n";
            PIT[dataPacket.contentName] = "dummy";
        } else {
            std::cout << "[BaselineNDN] No PIT entry for \"" << dataPacket.contentName << "\"\n";
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> diff = end - start;
        metrics.dataLatencies.push_back(diff.count());
        metrics.totalOperations++;
        
        // Update memory usage
        size_t currentMemory = getCurrentMemoryUsage();
        metrics.peakMemoryUsage = std::max(metrics.peakMemoryUsage, currentMemory);
    }

    bool serve_content(Content &servedContent) {
        auto start = std::chrono::high_resolution_clock::now();
        
        bool success = false;
        if (!CS.empty()) {
            std::string contentStr = CS.back();
            CS.pop_back();
            
            size_t pos = contentStr.find(":");
            if (pos != std::string::npos) {
                servedContent.name = contentStr.substr(0, pos);
                servedContent.data = contentStr.substr(pos + 1);
                std::cout << "[BaselineNDN] Serving content \"" << servedContent.name << "\"\n";
                success = true;
            }
        } else {
            std::cout << "[BaselineNDN] No content to serve.\n";
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> diff = end - start;
        metrics.retrievalLatencies.push_back(diff.count());
        metrics.totalOperations++;
        
        // Update memory usage
        size_t currentMemory = getCurrentMemoryUsage();
        metrics.peakMemoryUsage = std::max(metrics.peakMemoryUsage, currentMemory);
        
        return success;
    }
    
    const PerformanceMetrics& getMetrics() const {
        return metrics;
    }
    
    void resetMetrics() {
        metrics.clear();
    }
    
    void startMetricCollection() {
        metrics.clear();
    }
    
    void stopMetricCollection(double elapsedTimeSeconds) {
        metrics.totalTimeSeconds = elapsedTimeSeconds;
    }
};

// -------------------------
// Workload Generator
// -------------------------
class WorkloadGenerator {
private:
    std::vector<std::string> contentNames;
    std::vector<std::string> consumerIds;
    std::mt19937 rng;

public:
    WorkloadGenerator(int seed = 42) : rng(seed) {
        // Generate realistic content prefixes
        contentNames = {
            "/videos/popular/video1",
            "/videos/news/breaking",
            "/images/photos/vacation",
            "/text/articles/science",
            "/apps/downloads/game",
            "/streaming/live/sports",
            "/social/profiles/user",
            "/data/weather/forecast",
            "/content/music/top10",
            "/example/test/data"
        };
        
        // Generate consumer IDs
        for (int i = 1; i <= 20; i++) {
            consumerIds.push_back("consumer_" + std::to_string(i));
        }
    }
    
    InterestPacket generateInterest() {
        std::uniform_int_distribution<> contentDist(0, contentNames.size() - 1);
        std::uniform_int_distribution<> consumerDist(0, consumerIds.size() - 1);
        
        return InterestPacket{
            contentNames[contentDist(rng)],
            consumerIds[consumerDist(rng)]
        };
    }
    
    DataPacket generateData(const std::string& contentName) {
        // Generate random data payload (simulating different content sizes)
        std::uniform_int_distribution<> sizeDist(100, 1000);
        int dataSize = sizeDist(rng);
        std::string data(dataSize, 'X'); // Placeholder data
        
        return DataPacket{contentName, data};
    }
};

// -------------------------
// Performance Testing Functions
// -------------------------
void run_comparison_benchmark(int numOperations = 100) {
    std::cout << "\n=========== COMPARISON BENCHMARK ===========\n";
    std::cout << "Running " << numOperations << " operations on each router type\n";
    
    // Setup workload generator
    WorkloadGenerator workloadGen;
    
    // Setup routers
    NDNRouter obliviousRouter(true);
    BaselineNDNRouter baselineRouter(true);
    
    // Benchmark oblivious router
    std::cout << "\nBenchmarking privacy-preserving NDN router...\n";
    auto startOblivious = std::chrono::high_resolution_clock::now();
    obliviousRouter.startMetricCollection();
    
    for (int i = 0; i < numOperations; i++) {
        if (i % 1000 == 0) {
            std::cout << "Progress: " << i << "/" << numOperations << "\n";
        }
        
        InterestPacket interest = workloadGen.generateInterest();
        obliviousRouter.handle_interest(interest);
        
        DataPacket data = workloadGen.generateData(interest.contentName);
        obliviousRouter.handle_data(data);
        
        Content content;
        obliviousRouter.serve_content(content);
    }
    
    auto endOblivious = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diffOblivious = endOblivious - startOblivious;
    obliviousRouter.stopMetricCollection(diffOblivious.count());
    
    // Benchmark baseline router
    std::cout << "\nBenchmarking baseline NDN router...\n";
    auto startBaseline = std::chrono::high_resolution_clock::now();
    baselineRouter.startMetricCollection();
    
    for (int i = 0; i < numOperations; i++) {
        if (i % 1000 == 0) {
            std::cout << "Progress: " << i << "/" << numOperations << "\n";
        }
        
        InterestPacket interest = workloadGen.generateInterest();
        baselineRouter.handle_interest(interest);
        
        DataPacket data = workloadGen.generateData(interest.contentName);
        baselineRouter.handle_data(data);
        
        Content content;
        baselineRouter.serve_content(content);
    }
    
    auto endBaseline = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diffBaseline = endBaseline - startBaseline;
    baselineRouter.stopMetricCollection(diffBaseline.count());
    
    // Print results
    obliviousRouter.getMetrics().printSummary("Privacy-Preserving NDN Router Results");
    baselineRouter.getMetrics().printSummary("Baseline NDN Router Results");
    
    // Calculate overhead
    double throughputOblivious = obliviousRouter.getMetrics().totalOperations / obliviousRouter.getMetrics().totalTimeSeconds;
    double throughputBaseline = baselineRouter.getMetrics().totalOperations / baselineRouter.getMetrics().totalTimeSeconds;
    
    double avgLatencyOblivious = 0.0;
    if (!obliviousRouter.getMetrics().interestLatencies.empty()) {
        avgLatencyOblivious = std::accumulate(
            obliviousRouter.getMetrics().interestLatencies.begin(),
            obliviousRouter.getMetrics().interestLatencies.end(), 0.0
        ) / obliviousRouter.getMetrics().interestLatencies.size();
    }
    
    double avgLatencyBaseline = 0.0;
    if (!baselineRouter.getMetrics().interestLatencies.empty()) {
        avgLatencyBaseline = std::accumulate(
            baselineRouter.getMetrics().interestLatencies.begin(),
            baselineRouter.getMetrics().interestLatencies.end(), 0.0
        ) / baselineRouter.getMetrics().interestLatencies.size();
    }
    
    std::cout << "\n===== PERFORMANCE COMPARISON =====\n";
    std::cout << "Throughput overhead: " << (throughputBaseline / throughputOblivious) << "x\n";
    std::cout << "Latency overhead: " << (avgLatencyOblivious / avgLatencyBaseline) << "x\n";
    std::cout << "Memory overhead: " << 
        (obliviousRouter.getMetrics().peakMemoryUsage / 
         static_cast<double>(baselineRouter.getMetrics().peakMemoryUsage)) << "x\n";
    
    // Save results to CSV for further analysis
    obliviousRouter.getMetrics().saveToCSV("results/oblivious_router_metrics.csv");
    baselineRouter.getMetrics().saveToCSV("results/baseline_router_metrics.csv");
}

// -------------------------
// Tree Height Impact Test
// -------------------------
void run_tree_height_impact_test() {
    std::cout << "\n=========== TREE HEIGHT IMPACT TEST ===========\n";
    
    const int numOperations = 100;
    const std::vector<int> treeHeights = {3, 4, 5, 6, 7, 8}; // Different tree heights to test
    
    std::cout << "Testing impact of tree height on performance...\n";
    std::cout << "Running " << numOperations << " operations for each tree height\n";
    
    // Setup workload generator
    WorkloadGenerator workloadGen;
    
    std::ofstream resultFile("reuslts/tree_height_impact.csv");
    resultFile << "TreeHeight,ThroughputOpsPerSec,AvgInterestLatencyMicros,AvgDataLatencyMicros,AvgRetrievalLatencyMicros\n";
    
    for (int height : treeHeights) {
        std::cout << "\nTesting tree height = " << height << "\n";
        
        // Create router with specific tree height
        NDNRouter router(true, height, height - 1); // Queue height is 1 less than map height
        
        auto start = std::chrono::high_resolution_clock::now();
        router.startMetricCollection();
        
        for (int i = 0; i < numOperations; i++) {
            InterestPacket interest = workloadGen.generateInterest();
            router.handle_interest(interest);
            
            DataPacket data = workloadGen.generateData(interest.contentName);
            router.handle_data(data);
            
            Content content;
            router.serve_content(content);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        router.stopMetricCollection(diff.count());
        
        // Calculate metrics
        double throughput = router.getMetrics().totalOperations / router.getMetrics().totalTimeSeconds;
        
        double avgInterestLatency = 0.0;
        if (!router.getMetrics().interestLatencies.empty()) {
            avgInterestLatency = std::accumulate(
                router.getMetrics().interestLatencies.begin(),
                router.getMetrics().interestLatencies.end(), 0.0
            ) / router.getMetrics().interestLatencies.size();
        }
        
        double avgDataLatency = 0.0;
        if (!router.getMetrics().dataLatencies.empty()) {
            avgDataLatency = std::accumulate(
                router.getMetrics().dataLatencies.begin(),
                router.getMetrics().dataLatencies.end(), 0.0
            ) / router.getMetrics().dataLatencies.size();
        }
        
        double avgRetrievalLatency = 0.0;
        if (!router.getMetrics().retrievalLatencies.empty()) {
            avgRetrievalLatency = std::accumulate(
                router.getMetrics().retrievalLatencies.begin(),
                router.getMetrics().retrievalLatencies.end(), 0.0
            ) / router.getMetrics().retrievalLatencies.size();
        }
        
        std::cout << "Throughput: " << throughput << " ops/sec\n";
        std::cout << "Avg Interest Latency: " << avgInterestLatency << " μs\n";
        std::cout << "Avg Data Latency: " << avgDataLatency << " μs\n";
        std::cout << "Avg Retrieval Latency: " << avgRetrievalLatency << " μs\n";
        
        resultFile << height << ","
                   << throughput << ","
                   << avgInterestLatency << ","
                   << avgDataLatency << ","
                   << avgRetrievalLatency << "\n";
    }
    
    resultFile.close();
    std::cout << "\nTree height impact results saved to tree_height_impact.csv\n";
}

// -------------------------
// Bucket Size Impact Test
// -------------------------
void run_bucket_size_impact_test() {
    std::cout << "\n=========== BUCKET SIZE IMPACT TEST ===========\n";
    std::cout << "This test requires modifying the code to allow dynamic bucket sizes.\n";
    std::cout << "In your implementation, you would need to modify tree-map.hpp and tree-queue.hpp\n";
    std::cout << "to make BUCKET_CAPACITY and QUEUE_BUCKET_CAPACITY parameters instead of constants.\n";
    std::cout << "After implementing that change, you could create a similar test to tree_height_impact_test\n";
    std::cout << "that varies bucket sizes instead of tree heights.\n";
}

// -------------------------
// Concurrency Impact Test
// -------------------------
void run_concurrency_test(int maxThreads = 8) {
    std::cout << "\n=========== CONCURRENCY IMPACT TEST ===========\n";
    
    const int operationsPerThread = 20;
    std::vector<int> threadCounts = {1, 2, 4, 8, 16}; // Different thread counts to test
    
    // Don't test more threads than specified
    threadCounts.erase(
        std::remove_if(threadCounts.begin(), threadCounts.end(), 
                      [maxThreads](int tc) { return tc > maxThreads; }),
        threadCounts.end()
    );
    
    std::cout << "Testing impact of concurrency on performance...\n";
    
    std::ofstream resultFile("results/concurrency_impact.csv");
    resultFile << "ThreadCount,TotalOperations,TotalTimeSeconds,ThroughputOpsPerSec\n";
    
    for (int threadCount : threadCounts) {
        std::cout << "\nTesting with " << threadCount << " threads\n";
        
        // Create shared router
        NDNRouter router(true);
        
        // Setup workload generator
        WorkloadGenerator workloadGen;
        
        auto start = std::chrono::high_resolution_clock::now();
        router.startMetricCollection();
        
        // Create and run threads
        std::vector<std::thread> threads;
        for (int t = 0; t < threadCount; t++) {
            threads.emplace_back([&router, &workloadGen, operationsPerThread, t]() {
                for (int i = 0; i < operationsPerThread; i++) {
                    InterestPacket interest = workloadGen.generateInterest();
                    interest.consumerId += "_thread" + std::to_string(t); // Make unique
                    router.handle_interest(interest);
                    
                    DataPacket data = workloadGen.generateData(interest.contentName);
                    router.handle_data(data);
                    
                    Content content;
                    router.serve_content(content);
                }
            });
        }
        
        // Wait for all threads to finish
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        router.stopMetricCollection(diff.count());
        
        // Calculate metrics
        int totalOps = threadCount * operationsPerThread * 3; // 3 operations per iteration
        double throughput = totalOps / router.getMetrics().totalTimeSeconds;
        
        std::cout << "Total operations: " << totalOps << "\n";
        std::cout << "Total time: " << router.getMetrics().totalTimeSeconds << " seconds\n";
        std::cout << "Throughput: " << throughput << " ops/sec\n";
        
        resultFile << threadCount << ","
                   << totalOps << ","
                   << router.getMetrics().totalTimeSeconds << ","
                   << throughput << "\n";
    }
    
    resultFile.close();
    std::cout << "\nConcurrency impact results saved to concurrency_impact.csv\n";
}

// Forward Declarations
void router_thread_func(NDNRouter& router, int thread_id);
void run_unit_tests();
void run_profiling_test();
void run_integration_test();
void run_comparison_benchmark(int numOperations);
void run_tree_height_impact_test();
void run_bucket_size_impact_test();
void run_concurrency_test(int maxThreads);

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
        } else if (mode == "benchmark") {
            int numOperations = 1000; // Default
            if (argc > 2) {
                numOperations = std::stoi(argv[2]);
            }
            run_comparison_benchmark(numOperations);
            return 0;
        } else if (mode == "treeheight") {
            run_tree_height_impact_test();
            return 0;
        } else if (mode == "concurrency") {
            int maxThreads = 8; // Default
            if (argc > 2) {
                maxThreads = std::stoi(argv[2]);
            }
            run_concurrency_test(maxThreads);
            return 0;
        } else if (mode == "integration") {
            run_integration_test();
            return 0;
        }
    }
    
    // Default: run the parallel test harness
    try {
        std::cout << "Running parallel test harness with performance metrics...\n";
        NDNRouter router(true);
        const int num_threads = 4;
        std::vector<std::thread> threads;
        
        auto start = std::chrono::high_resolution_clock::now();
        router.startMetricCollection();
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(router_thread_func, std::ref(router), i+1);
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        router.stopMetricCollection(diff.count());
        
        router.getMetrics().printSummary("Parallel Test Results");
        
    } catch (const std::exception& ex) {
        std::cerr << "Main error: " << ex.what() << "\n";
    }
    return 0;
}

// -------------------------
// Existing Functions (with minor modifications)
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

void run_profiling_test() {
    std::cout << "Running profiling tests...\n";
    NDNRouter router(true); // Enable metrics collection
    const int iterations = 100;
    
    auto start = std::chrono::high_resolution_clock::now();
    router.startMetricCollection();
    
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
    router.stopMetricCollection(diff.count());
    
    std::cout << "Average time per iteration: " << (diff.count() / iterations) * 1000 << " ms\n";
    router.getMetrics().printSummary("Profiling Results");
    router.getMetrics().saveToCSV("profiling_results.csv");
}

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