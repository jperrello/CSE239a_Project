import random
import time
import heapq
import matplotlib.pyplot as plt
from collections import deque, defaultdict

class InterestPacket:
    def __init__(self, user_id, content_name, timestamp):
        self.user_id = user_id
        self.content_name = content_name
        self.timestamp = timestamp 
    
    def __repr__(self):
        return f"InterestPacket(user={self.user_id}, content={self.content_name}, time={self.timestamp})"
    
    # Define comparison for heapq - online this is how it is typically implemented
    def __lt__(self, other):
        return self.timestamp < other.timestamp  

class DeferredRetrievalSimulator:
    def __init__(self, users, max_delay=5):
        self.users = users 
        self.request_queue = deque()
        self.fetch_heap = []  # Min-heap for delayed fetches
        self.max_delay = max_delay  # random delay for deferred retrieval
        self.time = 0 
        self.fetch_delays = defaultdict(list)  # store delays 
        self.icn_cache = {}  # ICN cache

    def send_interest(self, user_id, content_name):
        """User sends an interest packet to PBACN."""
        # Check ICN cache first
        if content_name in self.icn_cache:
            print(f"{self.time}s: User {user_id} retrieved {content_name} from cache.")
            return
        
        packet = InterestPacket(user_id, content_name, self.time)
        self.request_queue.append(packet)
        
        # Schedule a deferred retrieval with randomized delay (SPARTA code will change this)
        fetch_time = self.time + random.randint(1, self.max_delay)
        heapq.heappush(self.fetch_heap, (fetch_time, packet))
        print(f"{self.time}s: User {user_id} sent interest for {content_name}. Fetch scheduled at {fetch_time}s")

    def process_fetches(self):
        """Process deferred fetches based on the current time."""
        while self.fetch_heap and self.fetch_heap[0][0] <= self.time:
            fetch_time, packet = heapq.heappop(self.fetch_heap)
            delay = fetch_time - packet.timestamp
            self.fetch_delays[packet.user_id].append(delay)
            self.icn_cache[packet.content_name] = fetch_time  # Store content in ICN cache
            print(f"{fetch_time}s: Fetching interest packet {packet} (delayed by {delay}s). Cached for future use.")

    def run_simulation(self, num_requests=10):
        """Run the event-driven simulation."""
        for _ in range(num_requests):
            user_id = random.choice(self.users)
            content_name = f"Content-{random.randint(1, 5)}"
            self.send_interest(user_id, content_name)
            
            # Simulation of time passing
            self.time += 1
            self.process_fetches()
            time.sleep(0.5)  
        
        self.analyze_security()
    
    def analyze_security(self):
        """Analyze the effect of deferred retrieval on traffic patterns."""
        all_delays = []
        for user, delays in self.fetch_delays.items():
            print(f"User {user} - Fetch Delays: {delays}")
            all_delays.extend(delays)
        
        # Plot script
        plt.hist(all_delays, bins=range(1, self.max_delay+2), edgecolor='black', alpha=0.7)
        plt.xlabel("Fetch Delay (Seconds)")
        plt.ylabel("Frequency")
        plt.title("Distribution of Fetch Delays (Traffic Obfuscation)")
        plt.show()

# Main
simulator = DeferredRetrievalSimulator(users=["Alice", "Bob", "Charlie"], max_delay=5)
simulator.run_simulation(num_requests=10)
