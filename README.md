# Setting up the Docker Container

to build the Docker container:   `docker build -t ndn-privacy-test .`
to run:     `docker run -it --rm -v "$(pwd)/results:/app/results" ndn-privacy test`

# Running Experiments using the docker Container

`./tree-test <mode> [options]`
Modes:
    operations       - Test with different operation counts (100-10000)
    configurations   - Test with different ORAM configurations
    comparison       - Compare with baseline implementation
    full             - Run all benchmark tests
    custom (th) (bc) (sl) (ops) - Run with custom parameters:
                    (th): Tree height
                    (bc): Bucket capacity
                    (sl): Stash limit
                    (ops): Number of operations


# Deferred Retrieval in PBACN-ICN

Our goal is to implement deferred retrieval in the context of PBACN-ICN, and modify how interest packets are fetched to obscure traffic patterns and prevent correlation attacks.

---

## Step 1: Choosing a Programming Language and Simulation Framework

Since this project is related to network routing and security, we needed to pick a language and framework that can handle:
1. Network Packet Simulation for interest packets and PBACN routing
2. Event-driven scheduling (the goal of deferred retrieval)
3. Performance testing

The choice we ended up going through with was **Python and Custom Event-Driven Simulation**.  
The benefits for this method are as follows:
1. This method is easier to prototype than C++ or OMNeT++.
2. Python queue structures allow for simple simulation of interest packets and deferred retrieval.

If we need to scale up later in the quarter, we can integrate this code into **ndnSIM** because Python allows for that interface.

---

## Step 2: Defining the Baseline

The baseline for this project should include:
1. Simulation of a basic ICN network with interest packets and PBACN routing.
2. Deferred retrieval, also known as randomly delay the fetching of interest packets.
3. Compare the two cases:
   - Without deferred retrieval
   - With deferred retrieval
4. Measure traffic correlation by checking whether fetching patterns are predictable.

---

## Explanation of Baseline Code

### `InterestPacket` Class:
Represents a request for content with a timestamp.

### `DeferredRetrievalSimulator` Class:
- **send_interest()**: A user sends an interest packet, which is scheduled for retrieval with a random delay. Given of course the packet isn't in the cache already.
- **process_fetches()**: Fetches interest packets based on the current time. Also simulates ICN nodes and caches.
- **run_simulation()**: Simulates a sequence of interest requests with deferred retrieval.
- **analyze_security()**: Stores the delay for each user's fetch request, and outputs delay data for each user.

We also added plotting functionality for a histogram of fetch delays to visualize how well traffic is obfuscated.  
This is mainly for testing as Joey likes to see data visualized on a graph.

---

## Why This Baseline Works

- We are able to effectively analyze how retrieval times vary between Immediate and Deferred Fetching.
- We successfully achieved traffic pattern obfuscation because requests are sent at known times, but retrievals are delayed randomly, breaking timing correlation.
- Fetch delays are widely spread, making it hard for an adversary to correlate traffic.
- We simulated ICN’s core advantage of reducing redundant network traffic through the use of ICN cache implementation.
- Additionally, through the use of caches, traffic is harder to correlate because some packets never go through the network.

---

## Oblivious Data Structs
The following oblivious data structures are for Named Data Networking (NDN), focusing on security and privacy. The key components include an oblivious map, an oblivious queue, and unit tests for an NDN router. These files were designed using concepts outlined in SPARTA for inspiration.

1. ob-map.hpp - Oblivious Map for Secure FIB & PIT Operations

    Implements an oblivious hash map to enhance security in NDN architectures.
    Used for:
        FIB (Forwarding Information Base): Hides the mapping between content names and forwarding interfaces to prevent traffic analysis.
        PIT (Pending Interest Table): Tracks outstanding interest packets while preventing timing-based attacks.
        
    Security Features:
        Dummy operations before and after real accesses to obscure patterns.
        Uses secure randomization to prevent adversary inference.
        Supports potential integration with cryptographic techniques like ORAM.

2. ob-queue.hpp - Oblivious Queue for Secure Content Caching (CS)

    Implements an oblivious queue for content caching in NDN's Content Store (CS).

    Designed to make push and pop operations indistinguishable through:
        Randomized memory accesses to prevent timing-based inference.
        Fixed-size circular buffer for efficient storage and retrieval.
        Pre- and post-dummy operations to balance performance and security.

3. test-ndn-router.cpp - Unit Tests for NDN Router

    Implements GoogleTest-based unit tests for:
        ObliviousMap (FIB & PIT): Tests insert, lookup, and removal security.
        ObliviousQueue (CS): Ensures queue operations behave as expected, even under full or empty conditions.
        NDN Router simulation: Tests how an NDN router processes interest and data packets using oblivious structures.

    Integration Test: Simulates an NDN router handling:
        Interest packets (checking PIT & FIB behavior).
        Data packets (testing PIT expiration and CS caching).
        Content serving (ensuring secure queue operations).

## ORAM Implementation
The code files outlined in this section were inspired by the articles and class content from weeks seven and eight. The files attempt to use ORAM techniques on the NDN router.

1. crypto.hpp - Cryptographic Utility for Secure Data Handling

Implements cryptographic primitives to secure stored data in NDN routers. Provides encryption and authentication for data blocks.

    AES-256-CBC Encryption: Ensures secure data storage.
    HMAC-SHA256 Integrity Check: Protects against data tampering.
    Key Management: Generates secure random keys and IVs.

Used in:

    tree-map.hpp (to encrypt stored key-value pairs in FIB & PIT).
    tree-queue.hpp (to encrypt cached content in the Content Store).

2. tree-map.hpp - Oblivious Map for Secure FIB & PIT Operations

Implements an oblivious hash map using a PathORAM-inspired structure to enhance privacy in NDN.

    Used for:
        FIB (Forwarding Information Base): Hides the mapping between content names and forwarding interfaces to prevent traffic analysis.
        PIT (Pending Interest Table): Stores interest packets while preventing access pattern leakage.

    Security Features:
        Reads and writes entire access paths to prevent leakage.
        Uses a stash to temporarily hold blocks before eviction.
        Randomized leaf remapping to prevent repeated lookup pattern inference.

3. tree-queue.hpp - Oblivious Queue for Secure Content Caching (CS)

Implements an oblivious queue for the Content Store (CS) in an NDN router.

    Designed to make push and pop operations indistinguishable:
        Uses randomized access patterns to prevent timing inference.
        Dummy operations interleaved with real reads and writes.
        Implements a PathORAM-based eviction strategy.

    Security Features:
        Oblivious Push: Encrypts and inserts data while hiding order.
        Oblivious Pop: Retrieves data without revealing request patterns.

4. tree-test.cpp - Unit Tests & Simulation for the NDN Router

Implements unit tests and a network simulation for the oblivious data structures.

    NDNRouter Simulation:
        Uses ObliviousMap for FIB & PIT operations.
        Uses ObliviousQueue for CS caching.
        Processes Interest Packets and Data Packets securely.

    Testing Includes:
        Unit Tests: Validate oblivious insertions, lookups, and caching.
        Profiling Tests: Measures performance over thousands of iterations.
        Integration Tests: Simulates real-world interest/data flow in an NDN router.
        Parallel Testing: Runs multiple router instances in separate threads.
        
This ensures NDN consumer privacy through the following principles:

    Oblivious FIB & PIT (tree-map.hpp): Prevents inference attacks on routing and interest tracking.
    Oblivious Content Store (tree-queue.hpp): Hides content caching and retrieval patterns.
    Strong Cryptography (crypto.hpp): Ensures data integrity and confidentiality.
    Performance Optimization: Balances security with efficient router operation.