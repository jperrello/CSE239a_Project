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
#include <sstream>

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
// Configuration Structure
// -------------------------
struct ORAMConfig {
    // Tree Map parameters
    int treeHeight;
    int bucketCapacity;
    size_t stashLimit;
    
    // Tree Queue parameters
    int queueTreeHeight;
    int queueBucketCapacity;
    size_t queueStashLimit;
    
    // Constructor with default values
    ORAMConfig(
        int tHeight = TREE_HEIGHT_DEFAULT,
        int bCapacity = BUCKET_CAPACITY_DEFAULT,
        size_t sLimit = STASH_LIMIT_DEFAULT,
        int qHeight = QUEUE_TREE_HEIGHT_DEFAULT,
        int qCapacity = QUEUE_BUCKET_CAPACITY_DEFAULT,
        size_t qLimit = QUEUE_STASH_LIMIT_DEFAULT
    ) : treeHeight(tHeight),
        bucketCapacity(bCapacity),
        stashLimit(sLimit),
        queueTreeHeight(qHeight),
        queueBucketCapacity(qCapacity),
        queueStashLimit(qLimit) {}
        
    // Method to create a string representation of the config
    std::string toString() const {
        std::stringstream ss;
        ss << "Map(h=" << treeHeight << ",b=" << bucketCapacity << ",s=" << stashLimit << ")_"
           << "Queue(h=" << queueTreeHeight << ",b=" << queueBucketCapacity << ",s=" << queueStashLimit << ")";
        return ss.str();
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

// -------------------------
// NDNRouter Class Using Oblivious Structures
// -------------------------
class NDNRouter {
private:
    ObliviousMap<std::string, std::string> FIB;
    ObliviousMap<std::string, std::string> PIT;
    ObliviousQueue<std::string> CS;
    PerformanceMetrics metrics;
    ORAMConfig config;

public:
    NDNRouter(bool collectMetrics = false, const ORAMConfig& oramConfig = ORAMConfig())
      : FIB(oramConfig.treeHeight, oramConfig.stashLimit, oramConfig.bucketCapacity),
        PIT(oramConfig.treeHeight, oramConfig.stashLimit, oramConfig.bucketCapacity),
        CS(oramConfig.queueTreeHeight, oramConfig.queueStashLimit, oramConfig.queueBucketCapacity),
        config(oramConfig)
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
        
        // Update stash metrics
        size_t fibStashSize = FIB.getStashSize();
        size_t pitStashSize = PIT.getStashSize();
        size_t totalStashSize = fibStashSize + pitStashSize;
        metrics.stashSizeHistory.push_back(totalStashSize);
        metrics.maxStashSize = std::max(metrics.maxStashSize, totalStashSize);
        
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
        
        // Update stash metrics
        size_t pitStashSize = PIT.getStashSize();
        size_t csStashSize = CS.getStashSize();
        size_t totalStashSize = pitStashSize + csStashSize;
        metrics.stashSizeHistory.push_back(totalStashSize);
        metrics.maxStashSize = std::max(metrics.maxStashSize, totalStashSize);
        
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
        
        // Update stash metrics
        size_t csStashSize = CS.getStashSize();
        metrics.stashSizeHistory.push_back(csStashSize);
        metrics.maxStashSize = std::max(metrics.maxStashSize, csStashSize);
        
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
    
    const ORAMConfig& getConfig() const {
        return config;
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
// Configuration Benchmark
// -------------------------
void run_configuration_benchmark(const std::vector<ORAMConfig>& configs, int numOperations) {
    std::cout << "\n=========== CONFIGURATION BENCHMARK ===========\n";
    std::cout << "Testing " << configs.size() << " different ORAM configurations with " 
              << numOperations << " operations each\n";
    
    // Prepare CSV file for results
    std::ofstream resultsFile("results/config_benchmark_results.csv");
    resultsFile << "TreeHeight,BucketCapacity,StashLimit,QueueTreeHeight,QueueBucketCapacity,QueueStashLimit,"
                << "Throughput,AvgInterestLatency,AvgDataLatency,AvgRetrievalLatency,MaxStashSize,TotalTimeSeconds\n";
    
    // Setup workload generator
    WorkloadGenerator workloadGen;
    
    for (const auto& config : configs) {
        std::cout << "\nTesting configuration: Tree height=" << config.treeHeight
                  << ", Bucket capacity=" << config.bucketCapacity
                  << ", Stash limit=" << config.stashLimit << "\n";
        
        try {
            // Create router with this configuration
            NDNRouter router(true, config);
            
            auto start = std::chrono::high_resolution_clock::now();
            router.startMetricCollection();
            
            for (int i = 0; i < numOperations; i++) {
                if (i % 100 == 0 && i > 0) {
                    std::cout << "Completed " << i << "/" << numOperations << " operations\r";
                    std::cout.flush();
                }
                
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
            
            size_t maxStashSize = router.getMetrics().maxStashSize;
            
            // Print results
            std::cout << "Throughput: " << throughput << " ops/sec\n";
            std::cout << "Avg Interest Latency: " << avgInterestLatency << " μs\n";
            std::cout << "Avg Data Latency: " << avgDataLatency << " μs\n";
            std::cout << "Avg Retrieval Latency: " << avgRetrievalLatency << " μs\n";
            std::cout << "Max Stash Size: " << maxStashSize << " blocks\n";
            std::cout << "Total Time: " << router.getMetrics().totalTimeSeconds << " seconds\n";
            
            // Write to CSV
            resultsFile << config.treeHeight << ","
                       << config.bucketCapacity << ","
                       << config.stashLimit << ","
                       << config.queueTreeHeight << ","
                       << config.queueBucketCapacity << ","
                       << config.queueStashLimit << ","
                       << throughput << ","
                       << avgInterestLatency << ","
                       << avgDataLatency << ","
                       << avgRetrievalLatency << ","
                       << maxStashSize << ","
                       << router.getMetrics().totalTimeSeconds << "\n";
            
            // Save detailed metrics
            std::string filename = "config_th" + std::to_string(config.treeHeight) + 
                                 "_bc" + std::to_string(config.bucketCapacity) + 
                                 "_sl" + std::to_string(config.stashLimit) + ".csv";
            router.getMetrics().saveToCSV(filename);
            
        } catch (const std::exception& ex) {
            std::cerr << "ERROR with configuration (h=" << config.treeHeight 
                      << ", b=" << config.bucketCapacity 
                      << ", s=" << config.stashLimit 
                      << "): " << ex.what() << "\n";
                      
            resultsFile << config.treeHeight << ","
                       << config.bucketCapacity << ","
                       << config.stashLimit << ","
                       << config.queueTreeHeight << ","
                       << config.queueBucketCapacity << ","
                       << config.queueStashLimit << ","
                       << "ERROR: " << ex.what() << "\n";
        }
    }
    
    resultsFile.close();
    std::cout << "\nConfiguration benchmark complete. Results saved to config_benchmark_results.csv\n";
}

// -------------------------
// Comparison with Baseline
// -------------------------
void compare_with_baseline(const std::vector<int>& operationCounts) {
    std::cout << "\n=========== BASELINE COMPARISON ===========\n";
    std::cout << "Comparing privacy-preserving NDN with baseline implementation\n";
    
    // Create results file
    std::ofstream resultsFile("results/baseline_comparison.csv");
    resultsFile << "OperationCount,BaselineThroughput,PrivacyThroughput,ThroughputOverhead,"
                << "BaselineInterestLatency,PrivacyInterestLatency,InterestLatencyOverhead,"
                << "BaselineDataLatency,PrivacyDataLatency,DataLatencyOverhead,"
                << "BaselineRetrievalLatency,PrivacyRetrievalLatency,RetrievalLatencyOverhead,"
                << "BaselineMemoryMB,PrivacyMemoryMB,MemoryOverhead\n";
    
    // Setup workload generator
    WorkloadGenerator workloadGen;
    
    // Default configuration
    ORAMConfig defaultConfig;
    
    for (int opCount : operationCounts) {
        std::cout << "\nComparing with " << opCount << " operations...\n";
        
        try {
            // First run baseline
            std::cout << "Running baseline implementation...\n";
            BaselineNDNRouter baselineRouter(true);
            
            auto baselineStart = std::chrono::high_resolution_clock::now();
            baselineRouter.startMetricCollection();
            
            for (int i = 0; i < opCount; i++) {
                if (i % 100 == 0 && i > 0) {
                    std::cout << "Baseline: " << i << "/" << opCount << " operations\r";
                    std::cout.flush();
                }
                
                InterestPacket interest = workloadGen.generateInterest();
                baselineRouter.handle_interest(interest);
                
                DataPacket data = workloadGen.generateData(interest.contentName);
                baselineRouter.handle_data(data);
                
                Content content;
                baselineRouter.serve_content(content);
            }
            
            auto baselineEnd = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> baselineDiff = baselineEnd - baselineStart;
            baselineRouter.stopMetricCollection(baselineDiff.count());
            
            // Now run privacy-preserving implementation
            std::cout << "\nRunning privacy-preserving implementation...\n";
            NDNRouter privacyRouter(true, defaultConfig);
            
            auto privacyStart = std::chrono::high_resolution_clock::now();
            privacyRouter.startMetricCollection();
            
            for (int i = 0; i < opCount; i++) {
                if (i % 100 == 0 && i > 0) {
                    std::cout << "Privacy: " << i << "/" << opCount << " operations\r";
                    std::cout.flush();
                }
                
                InterestPacket interest = workloadGen.generateInterest();
                privacyRouter.handle_interest(interest);
                
                DataPacket data = workloadGen.generateData(interest.contentName);
                privacyRouter.handle_data(data);
                
                Content content;
                privacyRouter.serve_content(content);
            }
            
            auto privacyEnd = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> privacyDiff = privacyEnd - privacyStart;
            privacyRouter.stopMetricCollection(privacyDiff.count());
            
            // Calculate metrics
            double baselineThroughput = baselineRouter.getMetrics().totalOperations / 
                                        baselineRouter.getMetrics().totalTimeSeconds;
            
            double privacyThroughput = privacyRouter.getMetrics().totalOperations / 
                                       privacyRouter.getMetrics().totalTimeSeconds;
            
            double throughputOverhead = baselineThroughput / privacyThroughput;
            
            // Calculate average latencies
            auto calcAvg = [](const std::vector<double>& latencies) {
                return latencies.empty() ? 0.0 : 
                       std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
            };
            
            double baselineInterestLatency = calcAvg(baselineRouter.getMetrics().interestLatencies);
            double privacyInterestLatency = calcAvg(privacyRouter.getMetrics().interestLatencies);
            double interestLatencyOverhead = privacyInterestLatency / baselineInterestLatency;
            
            double baselineDataLatency = calcAvg(baselineRouter.getMetrics().dataLatencies);
            double privacyDataLatency = calcAvg(privacyRouter.getMetrics().dataLatencies);
            double dataLatencyOverhead = privacyDataLatency / baselineDataLatency;
            
            double baselineRetrievalLatency = calcAvg(baselineRouter.getMetrics().retrievalLatencies);
            double privacyRetrievalLatency = calcAvg(privacyRouter.getMetrics().retrievalLatencies);
            double retrievalLatencyOverhead = privacyRetrievalLatency / baselineRetrievalLatency;
            
            // Memory usage in MB
            double baselineMemoryMB = baselineRouter.getMetrics().peakMemoryUsage / (1024.0 * 1024.0);
            double privacyMemoryMB = privacyRouter.getMetrics().peakMemoryUsage / (1024.0 * 1024.0);
            double memoryOverhead = privacyMemoryMB / baselineMemoryMB;
            
            // Print comparison results
            std::cout << "\nResults for " << opCount << " operations:\n";
            std::cout << "Throughput: Baseline=" << baselineThroughput 
                     << " ops/sec, Privacy=" << privacyThroughput 
                     << " ops/sec, Overhead=" << throughputOverhead << "x\n";
                     
            std::cout << "Interest Latency: Baseline=" << baselineInterestLatency 
                     << " μs, Privacy=" << privacyInterestLatency 
                     << " μs, Overhead=" << interestLatencyOverhead << "x\n";
                     
            std::cout << "Data Latency: Baseline=" << baselineDataLatency 
                     << " μs, Privacy=" << privacyDataLatency 
                     << " μs, Overhead=" << dataLatencyOverhead << "x\n";
                     
            std::cout << "Retrieval Latency: Baseline=" << baselineRetrievalLatency 
                     << " μs, Privacy=" << privacyRetrievalLatency 
                     << " μs, Overhead=" << retrievalLatencyOverhead << "x\n";
                     
            std::cout << "Memory Usage: Baseline=" << baselineMemoryMB 
                     << " MB, Privacy=" << privacyMemoryMB 
                     << " MB, Overhead=" << memoryOverhead << "x\n";
            
            // Write to CSV
            resultsFile << opCount << ","
                       << baselineThroughput << ","
                       << privacyThroughput << ","
                       << throughputOverhead << ","
                       << baselineInterestLatency << ","
                       << privacyInterestLatency << ","
                       << interestLatencyOverhead << ","
                       << baselineDataLatency << ","
                       << privacyDataLatency << ","
                       << dataLatencyOverhead << ","
                       << baselineRetrievalLatency << ","
                       << privacyRetrievalLatency << ","
                       << retrievalLatencyOverhead << ","
                       << baselineMemoryMB << ","
                       << privacyMemoryMB << ","
                       << memoryOverhead << "\n";
            
            // Save detailed metrics
            baselineRouter.getMetrics().saveToCSV("baseline_" + std::to_string(opCount) + ".csv");
            privacyRouter.getMetrics().saveToCSV("privacy_" + std::to_string(opCount) + ".csv");
            
        } catch (const std::exception& ex) {
            std::cerr << "ERROR with " << opCount << " operations: " << ex.what() << "\n";
            resultsFile << opCount << ",ERROR: " << ex.what() << "\n";
        }
    }
    
    resultsFile.close();
    std::cout << "\nBaseline comparison complete. Results saved to baseline_comparison.csv\n";
}

// -------------------------
// Operations Benchmark
// -------------------------
void run_operations_benchmark(const std::vector<int>& operationCounts) {
    std::cout << "\n=========== OPERATIONS SCALING BENCHMARK ===========\n";
    std::cout << "Testing performance with different operation counts\n";
    
    // Default configuration
    ORAMConfig defaultConfig;
    
    // Create results file
    std::ofstream resultsFile("operations_benchmark.csv");
    resultsFile << "OperationCount,ThroughputOpsPerSec,InterestLatencyMean,DataLatencyMean,RetrievalLatencyMean,MaxStashSize,TotalTimeSeconds\n";
    
    // Setup workload generator
    WorkloadGenerator workloadGen;
    
    for (int opCount : operationCounts) {
        std::cout << "\nRunning benchmark with " << opCount << " operations...\n";
        
        try {
            // Create router with default configuration
            NDNRouter router(true, defaultConfig);
            
            auto start = std::chrono::high_resolution_clock::now();
            router.startMetricCollection();
            
            for (int i = 0; i < opCount; i++) {
                if (i % 100 == 0 && i > 0) {
                    std::cout << "Completed " << i << "/" << opCount << " operations\r";
                    std::cout.flush();
                }
                
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
            
            size_t maxStashSize = router.getMetrics().maxStashSize;
            
            // Print results
            std::cout << "Throughput: " << throughput << " ops/sec\n";
            std::cout << "Avg Interest Latency: " << avgInterestLatency << " μs\n";
            std::cout << "Avg Data Latency: " << avgDataLatency << " μs\n";
            std::cout << "Avg Retrieval Latency: " << avgRetrievalLatency << " μs\n";
            std::cout << "Max Stash Size: " << maxStashSize << " blocks\n";
            std::cout << "Total Time: " << router.getMetrics().totalTimeSeconds << " seconds\n";
            
            // Write to CSV
            resultsFile << opCount << ","
                       << throughput << ","
                       << avgInterestLatency << ","
                       << avgDataLatency << ","
                       << avgRetrievalLatency << ","
                       << maxStashSize << ","
                       << router.getMetrics().totalTimeSeconds << "\n";
            
            // Save detailed metrics
            std::string filename = "operations_" + std::to_string(opCount) + ".csv";
            router.getMetrics().saveToCSV(filename);
            
        } catch (const std::exception& ex) {
            std::cerr << "ERROR with " << opCount << " operations: " << ex.what() << "\n";
            resultsFile << opCount << ",ERROR: " << ex.what() << "\n";
        }
    }
    
    resultsFile.close();
    std::cout << "\nOperations benchmark complete. Results saved to operations_benchmark.csv\n";
}

// -------------------------
// Main Function: Dispatch based on Command-line Argument
// -------------------------
int main(int argc, char* argv[]) {
    // Setup default operational parameters
    std::vector<int> defaultOperationCounts = {100, 500, 1000, 5000, 10000};
    int defaultConfigTestOperations = 1000;
    
    // Parse command line arguments
    if (argc > 1) {
        std::string mode = argv[1];
        
        if (mode == "operations") {
            // Test with different operation counts
            run_operations_benchmark(defaultOperationCounts);
            return 0;
        }
        else if (mode == "configurations") {
            // Define configurations to test
            std::vector<ORAMConfig> configs;
            
            // Test different tree heights (keeping other parameters default)
            configs.push_back(ORAMConfig(4, BUCKET_CAPACITY_DEFAULT, STASH_LIMIT_DEFAULT, 3, QUEUE_BUCKET_CAPACITY_DEFAULT, QUEUE_STASH_LIMIT_DEFAULT));
            configs.push_back(ORAMConfig(5, BUCKET_CAPACITY_DEFAULT, STASH_LIMIT_DEFAULT, 4, QUEUE_BUCKET_CAPACITY_DEFAULT, QUEUE_STASH_LIMIT_DEFAULT));
            configs.push_back(ORAMConfig(6, BUCKET_CAPACITY_DEFAULT, STASH_LIMIT_DEFAULT, 5, QUEUE_BUCKET_CAPACITY_DEFAULT, QUEUE_STASH_LIMIT_DEFAULT));
            configs.push_back(ORAMConfig(7, BUCKET_CAPACITY_DEFAULT, STASH_LIMIT_DEFAULT, 6, QUEUE_BUCKET_CAPACITY_DEFAULT, QUEUE_STASH_LIMIT_DEFAULT));
            
            // Test different bucket capacities (keeping tree height at default)
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, 2, STASH_LIMIT_DEFAULT, QUEUE_TREE_HEIGHT_DEFAULT, 4, QUEUE_STASH_LIMIT_DEFAULT));
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, 4, STASH_LIMIT_DEFAULT, QUEUE_TREE_HEIGHT_DEFAULT, 8, QUEUE_STASH_LIMIT_DEFAULT));
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, 8, STASH_LIMIT_DEFAULT, QUEUE_TREE_HEIGHT_DEFAULT, 16, QUEUE_STASH_LIMIT_DEFAULT));
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, 16, STASH_LIMIT_DEFAULT, QUEUE_TREE_HEIGHT_DEFAULT, 32, QUEUE_STASH_LIMIT_DEFAULT));
            
            // Test different stash limits (keeping tree height and bucket capacity at default)
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, BUCKET_CAPACITY_DEFAULT, 50, QUEUE_TREE_HEIGHT_DEFAULT, QUEUE_BUCKET_CAPACITY_DEFAULT, 50));
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, BUCKET_CAPACITY_DEFAULT, 100, QUEUE_TREE_HEIGHT_DEFAULT, QUEUE_BUCKET_CAPACITY_DEFAULT, 100));
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, BUCKET_CAPACITY_DEFAULT, 200, QUEUE_TREE_HEIGHT_DEFAULT, QUEUE_BUCKET_CAPACITY_DEFAULT, 200));
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, BUCKET_CAPACITY_DEFAULT, 500, QUEUE_TREE_HEIGHT_DEFAULT, QUEUE_BUCKET_CAPACITY_DEFAULT, 500));
            
            // Run tests with these configurations
            run_configuration_benchmark(configs, defaultConfigTestOperations);
            return 0;
        }
        else if (mode == "comparison") {
            // Compare with baseline
            compare_with_baseline(defaultOperationCounts);
            return 0;
        }
        else if (mode == "full") {
            // Run all tests
            std::cout << "Running full benchmark suite...\n";
            
            // Test with different operation counts
            run_operations_benchmark(defaultOperationCounts);
            
            // Define configurations to test
            std::vector<ORAMConfig> configs;
            
            // Test different tree heights
            configs.push_back(ORAMConfig(4, BUCKET_CAPACITY_DEFAULT, STASH_LIMIT_DEFAULT, 3, QUEUE_BUCKET_CAPACITY_DEFAULT, QUEUE_STASH_LIMIT_DEFAULT));
            configs.push_back(ORAMConfig(5, BUCKET_CAPACITY_DEFAULT, STASH_LIMIT_DEFAULT, 4, QUEUE_BUCKET_CAPACITY_DEFAULT, QUEUE_STASH_LIMIT_DEFAULT));
            configs.push_back(ORAMConfig(6, BUCKET_CAPACITY_DEFAULT, STASH_LIMIT_DEFAULT, 5, QUEUE_BUCKET_CAPACITY_DEFAULT, QUEUE_STASH_LIMIT_DEFAULT));
            configs.push_back(ORAMConfig(7, BUCKET_CAPACITY_DEFAULT, STASH_LIMIT_DEFAULT, 6, QUEUE_BUCKET_CAPACITY_DEFAULT, QUEUE_STASH_LIMIT_DEFAULT));
            
            // Test different bucket capacities
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, 2, STASH_LIMIT_DEFAULT, QUEUE_TREE_HEIGHT_DEFAULT, 4, QUEUE_STASH_LIMIT_DEFAULT));
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, 4, STASH_LIMIT_DEFAULT, QUEUE_TREE_HEIGHT_DEFAULT, 8, QUEUE_STASH_LIMIT_DEFAULT));
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, 8, STASH_LIMIT_DEFAULT, QUEUE_TREE_HEIGHT_DEFAULT, 16, QUEUE_STASH_LIMIT_DEFAULT));
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, 16, STASH_LIMIT_DEFAULT, QUEUE_TREE_HEIGHT_DEFAULT, 32, QUEUE_STASH_LIMIT_DEFAULT));
            
            // Test different stash limits
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, BUCKET_CAPACITY_DEFAULT, 50, QUEUE_TREE_HEIGHT_DEFAULT, QUEUE_BUCKET_CAPACITY_DEFAULT, 50));
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, BUCKET_CAPACITY_DEFAULT, 100, QUEUE_TREE_HEIGHT_DEFAULT, QUEUE_BUCKET_CAPACITY_DEFAULT, 100));
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, BUCKET_CAPACITY_DEFAULT, 200, QUEUE_TREE_HEIGHT_DEFAULT, QUEUE_BUCKET_CAPACITY_DEFAULT, 200));
            configs.push_back(ORAMConfig(TREE_HEIGHT_DEFAULT, BUCKET_CAPACITY_DEFAULT, 500, QUEUE_TREE_HEIGHT_DEFAULT, QUEUE_BUCKET_CAPACITY_DEFAULT, 500));
            
            // Run tests with these configurations
            run_configuration_benchmark(configs, defaultConfigTestOperations);
            
            // Compare with baseline
            compare_with_baseline(defaultOperationCounts);
            
            std::cout << "\nFull benchmark suite completed.\n";
            return 0;
        }
        else if (mode == "custom") {
            if (argc < 6) {
                std::cerr << "Custom mode requires at least 5 arguments:\n";
                std::cerr << "tree-test custom <tree_height> <bucket_capacity> <stash_limit> <num_operations>\n";
                return 1;
            }
            
            int treeHeight = std::stoi(argv[2]);
            int bucketCapacity = std::stoi(argv[3]);
            int stashLimit = std::stoi(argv[4]);
            int numOperations = std::stoi(argv[5]);
            
            // Create custom configuration
            ORAMConfig customConfig(
                treeHeight, 
                bucketCapacity, 
                stashLimit, 
                treeHeight > 1 ? treeHeight - 1 : 1,  // Queue height is one less than map height
                bucketCapacity * 2,  // Queue bucket capacity is double the map bucket capacity
                stashLimit
            );
            
            std::vector<ORAMConfig> configs = {customConfig};
            run_configuration_benchmark(configs, numOperations);
            return 0;
        }
        else {
            std::cerr << "Unknown mode: " << mode << "\n";
        }
    }
    
    // Display usage information
    std::cout << "Usage: " << argv[0] << " <mode> [options]\n";
    std::cout << "Modes:\n";
    std::cout << "  operations       - Test with different operation counts (" 
              << defaultOperationCounts.front() << "-" << defaultOperationCounts.back() << ")\n";
    std::cout << "  configurations   - Test with different ORAM configurations\n";
    std::cout << "  comparison       - Compare with baseline implementation\n";
    std::cout << "  full             - Run all benchmark tests\n";
    std::cout << "  custom <th> <bc> <sl> <ops> - Run with custom parameters:\n";
    std::cout << "                    <th>: Tree height\n";
    std::cout << "                    <bc>: Bucket capacity\n";
    std::cout << "                    <sl>: Stash limit\n";
    std::cout << "                    <ops>: Number of operations\n";
    
    return 1;
};
