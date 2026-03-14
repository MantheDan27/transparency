## 2024-03-06 - Optimize ARP table lookups during network scan
**Learning:** In network scanning tools, resolving MAC addresses by executing a shell command (`cat /proc/net/arp` or `arp -n`) per IP address creates a massive bottleneck due to excessive process spawning. Spawning processes concurrently per IP in `Promise.all` slows down the entire system and main thread significantly.
**Action:** Always fetch and cache the entire ARP table once per scan batch instead of querying it per individual IP address. This turns O(N) process executions into O(1), improving performance by >100x for MAC resolution.

# 2026-03-07 - ARP Scanning Bottleneck

**Learning:** Resolving MAC addresses via individual shell commands (`arp -a <ip>`) per IP during network scans creates a massive process-spawning bottleneck, significantly slowing down the scan and consuming excess system resources.
**Action:** Fetch and cache the entire ARP table once per scan batch, looking up individual IPs from the in-memory Map.

## 2024-05-28 - C++ Network Scanner Threading Bottleneck
**Learning:** Spawning a new `std::thread` per task (e.g., per IP address during a ping sweep or per port during a scan) creates massive overhead and slows down scanning operations significantly, even when throttled by a semaphore/condition variable. The constant context-switching and thread allocation becomes a bottleneck.
**Action:** Always replace per-task thread creation with a fixed-size `std::thread` pool where threads are spawned once, and work is distributed safely by having each thread pull the next task using an `std::atomic<size_t>` index counter.
