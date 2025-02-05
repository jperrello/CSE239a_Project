Our goal is to implement deferred retrieval in the context of PBACN-ICN, and modify how interest packets are fetched to obscure traffic patterns and prevent correlation attacks.

================================================================
Step 1: Choosing a Programming Language and Simulation Framework
================================================================
Since this project is related to network routing and security, we needed to pick a language and framework that can handle:
    1. Network Packet Simulation for interest packets and PBACN routing
    2. Event driven scheduling (the goal of deferred retrieval)
    3. Performance testing

The choice we ended up going through with was Python and Custom Event-Driven Simulation.
The benefits for this method are as follows:
    1. This method is easier to prototype than C++ or OMNeT++.
    2. Python queue structures allow for simple simulation of interest packets and deferred retrieval.
If we need to scale up later in the quarter, we can integrate this code into ndnSIM because Python allows for that interface.

=============================
Step 2: Defining the Baseline
=============================
The baseline for this project should include:
    1. Simulation of a basic ICN network with interest packets and PBACN routing.
    2. Deferred retrieval, also known as randomly delay the fetching of interest packets.
    3. Compare the two cases:
        Without deferred retrieval
        With deferred retrieval 
    4. Measure traffic correlation by checking whether fetching patterns are predictable.

============================
Exxplanation of Current Code
============================

InterestPacket Class: 
Represents a request for content with a timestamp.

DeferredRetrievalSimulator Class:
    send_interest(): A user sends an interest packet, which is scheduled for retrieval with a random delay. Given of course the packet isnt in the cache already.
    process_fetches(): Fetches interest packets based on the current time. Also simulates ICN nodes and caches.
    run_simulation(): Simulates a sequence of interest requests with deferred retrieval.
    analyze_security(): Stores the delay for each user's fetch request, and outputs delay data for each user.

    We also added plotting functionality for a histogram of fetch delays to visualize how well traffic is obfuscated. 
    This is mainly for testing as Joey likes to see data visulized on a graph.

=======================
Why This Baseline Works
=======================
We are able to effectively analyze how retrieval times vary between Immediate and Deferred Fetching.
We successfully acheived traffic pattern obfuscation because requests are sent at known times, but retrievals are delayed randomly, breaking timing correlation.
Fetch delays are widely spread, making it hard for an adversary to correlate traffic.
We simulated ICN’s core advantage of reducing redundant network traffic through the use of ICN cache implementation.
Additionally, through the use of caches, traffic is harder to correlate because some packets never go through the network.

==========
Next Steps
==========
We need to scale this to realistic network simulations. We plan on using ndnSIM for this.