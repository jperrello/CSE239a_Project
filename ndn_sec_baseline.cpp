/*
 * This example demonstrates a baseline NDN consumer application that:
 *   - Expresses an Interest for named data (e.g., "/example/testApp/data")
 *   - Receives a Data packet from the network
 *   - Verifies the Data packet's signature using NDN’s built-in security (KeyChain)
 *
 * Aspects of this Implementation:
 *  - Named Data: Data is identified and requested by its name rather than by an endpoint address.
 *  - Interest and Data Packets: The core NDN packet types.
 *  - Built-in Security: Each Data packet is signed. The KeyChain component is used to verify the signature.
 *     -https://support.apple.com/en-gb/guide/security/secb0694df1a/1/web/1 for more info on keychain
 *
 * This baseline:
 *  - Illustrates how data packets can be fetched and verified.
 *  - Shows where and how trust management mechanisms (like additional key fetching or policy checks)
 *    would be integrated if the default trust model does not suffice.
 *
 *  - The ndn-cxx library handles many low-level details for us. In advanced applications,
 *    we may want to override or extend these behaviors to experiment with alternative trust models.
 *  
 * 
 * Compilation (assuming ndn-cxx is installed):
 *   g++ -std=c++11 -o ndn_consumer ndn_consumer.cpp $(pkg-config --cflags --libs ndn-cxx)
 */

 #include <ndn-cxx/face.hpp>
 #include <ndn-cxx/security/key-chain.hpp>
 #include <boost/asio.hpp>
 #include <iostream>
 #include <string>
 #include <functional>
 
 
 using namespace ndn;
 using namespace std;
 using namespace std::placeholders;  // For _1, _2 in std::bind
 
 // The NdnConsumer class encapsulates the functionality of a basic NDN consumer.
 class NdnConsumer {
 public:
   // Constructor: sets up the I/O service, Face (communication channel), and KeyChain (security).
   NdnConsumer()
     : m_face(m_ioService)
     , m_keyChain() //  KeyChain enables PKI signature verification.
   {
    // The KeyChain is preconfigured with trusted keys and certificates for fast verification.
     // In the future: In advanced scenarios, we could configure m_keyChain with a custom trust model or policy.
   }
 
   // The run() method starts the consumer by expressing an Interest for a specific named data.
   void run() {
     // Define the name of the data we wish to retrieve.

     Name dataName("/example/testApp/data");
 
     // Create an Interest packet with the specified name.
     Interest interest(dataName);
     interest.setInterestLifetime(time::seconds(2)); // Interest will be valid for 2 seconds.
     interest.setMustBeFresh(true); // Request only fresh data (i.e., not stale content).
 
     cout << "Expressing Interest for: " << dataName.toUri() << endl;
 
     // Express the Interest over the network.
     // Three callbacks are registered to handle:
     //  - Successful Data retrieval (onData)
     //  - Negative acknowledgments (Nacks) (onNack)
     //  - Timeouts (onTimeout)
     m_face.expressInterest(interest,
                            bind(&NdnConsumer::onData, this, _1, _2),
                            bind(&NdnConsumer::onNack, this, _1, _2),
                            bind(&NdnConsumer::onTimeout, this, _1));
 
     // Enter the event loop to process network events and callbacks.
     m_face.processEvents();
   }
 
 private:
   // Callback function called when a Data packet is successfully received.
   void onData(const Interest& interest, const Data& data) {
     cout << "Received Data: " << data.getName().toUri() << endl;
 
     // When data is received, its signature must be verified.
     // The KeyChain performs cryptographic verification based on the key locator
     // present in the Data packet (which indicates how to find the signing key).
     try {
       m_keyChain.verifyData(data,
                             // onVerified is called if the signature verification is successful.
                             bind(&NdnConsumer::onVerified, this, _1),
                             // onVerificationFailed is called if verification fails.
                             bind(&NdnConsumer::onVerificationFailed, this, _1, _2));
     }
     catch (const std::exception& e) {
       cerr << "Exception during signature verification: " << e.what() << endl;
     }
   }
 
   // Callback function when the Data packet's signature is successfully verified above.
   void onVerified(const Data& data) {
     cout << "Data signature verified successfully." << endl;
 
     // Process the Data payload.
     // For demonstration, we assume the payload is textual and print it.
     // In real applications, the payload might be binary or structured data.
     auto payload = data.getContent().blockFromValue();
     string payloadStr(reinterpret_cast<const char*>(payload.value()), payload.value_size());
     cout << "Data payload: " << payloadStr << endl;
 
     // Future: Here, further application logic can be implemented,
     // Future: such as content access control or additional key/trust management.
     // Currently, my solution to this is to launch asynchronous blockchain verification
     // Dont worry, this does not block any packet processing so throughput remains the same.
     verifyBlockchainAsync(data);
   }
 
   // Callback  when signature verification fails.
   void onVerificationFailed(const Data& data, const std::string& reason) {
     cerr << "Data signature verification failed: " << reason << endl;
     // Future: this is where alternative trust management strategies
     // could be applied, such as retrieving a new key, checking alternate trust paths,
     // or prompting for user intervention.
   }
 
   // Callback when a NACK (negative acknowledgment) is received.
   void onNack(const Interest& interest, const lp::Nack& nack) {
     cout << "Received Nack for Interest: " << interest.getName().toUri() << endl;
     // Here, you might analyze the Nack reason and decide whether to re-express the Interest.
   }
 
   // Callback function invoked when an Interest times out.
   void onTimeout(const Interest& interest) {
     cout << "Interest timeout for: " << interest.getName().toUri() << endl;
     // Timeout handling logic (such as retrying the Interest) can be added here.
   }

   // Asynchronous function to simulate a blockchain verification query.
   // In practice, this would interface with a blockchain or distributed ledger.
   void verifyBlockchainAsync(const Data& data) {
     // Launch a new thread to perform the blockchain check in the background.
     std::thread([this, data]() {
       // Simulate network delay for a blockchain query.
       std::this_thread::sleep_for(std::chrono::seconds(1));

       // Simulated blockchain verification logic.
       // This is where we would query the blockchain to see if it is successful
       // for now this is just labeled as true to say it was a successful query
       bool blockchainVerified = true; 
       
       if (blockchainVerified) {
         cout << "Blockchain verification successful for certificate from: "
              << data.getName().toUri() << endl;
       }
       else {
         cerr << "Blockchain verification failed for certificate from: "
              << data.getName().toUri() << endl;
         // Future: Optionally, trigger further error handling or certificate update procedures.
       }
     }).detach(); // Detach the thread so it runs independently.
   }
 
 private:
   boost::asio::io_service m_ioService;  // Asynchronous I/O service.
   Face m_face;                          // NDN Face used for network communication.
   KeyChain m_keyChain;                  // KeyChain for PKI verification.
 };
 
 int main() {
   try {
     // Instantiate the NDN consumer application.
     NdnConsumer consumer;
     // Start the consumer to express an Interest and process the incoming Data.
     consumer.run();
   }
   catch (const std::exception& e) {
     cerr << "ERROR: " << e.what() << endl;
   }
   return 0;
 }
 