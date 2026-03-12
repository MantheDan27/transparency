## 2024-03-06 - Optimize ARP table lookups during network scan
**Learning:** In network scanning tools, resolving MAC addresses by executing a shell command (`cat /proc/net/arp` or `arp -n`) per IP address creates a massive bottleneck due to excessive process spawning. Spawning processes concurrently per IP in `Promise.all` slows down the entire system and main thread significantly.
**Action:** Always fetch and cache the entire ARP table once per scan batch instead of querying it per individual IP address. This turns O(N) process executions into O(1), improving performance by >100x for MAC resolution.

# 2026-03-07 - ARP Scanning Bottleneck

**Learning:** Resolving MAC addresses via individual shell commands (`arp -a <ip>`) per IP during network scans creates a massive process-spawning bottleneck, significantly slowing down the scan and consuming excess system resources.
**Action:** Fetch and cache the entire ARP table once per scan batch, looking up individual IPs from the in-memory Map.

## 2026-03-08 - Optimize Port Scanning Thread Creation
**Learning:** In C++ network scanners (like `transparency-linux`), spawning a new `std::thread` for every single port being scanned (up to 65k ports) creates massive overhead and memory pressure, even if throttled by a condition variable.
**Action:** Use a fixed-size thread pool (e.g., `std::min(32, (int)ports.size())`) with a shared `std::atomic<size_t>` index to distribute task processing. This turns O(N) thread creations into O(1), significantly improving performance.
