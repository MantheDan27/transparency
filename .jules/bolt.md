## 2025-03-13 - [Debounce Optimization for Search Input]
**Learning:** Adding debouncing to frequent event handlers (like keystrokes) prevents massive synchronous UI blocking in large DOMs (like device tables).
**Action:** When working on frontends with large rendering scopes, implement a debounce wrapper to delay rendering triggers until input stabilizes. Ensure you don't accidentally pull in lockfile dependency changes while doing so.
## 2024-03-06 - Optimize ARP table lookups during network scan
**Learning:** In network scanning tools, resolving MAC addresses by executing a shell command (`cat /proc/net/arp` or `arp -n`) per IP address creates a massive bottleneck due to excessive process spawning. Spawning processes concurrently per IP in `Promise.all` slows down the entire system and main thread significantly.
**Action:** Always fetch and cache the entire ARP table once per scan batch instead of querying it per individual IP address. This turns O(N) process executions into O(1), improving performance by >100x for MAC resolution.

# 2026-03-07 - ARP Scanning Bottleneck

**Learning:** Resolving MAC addresses via individual shell commands (`arp -a <ip>`) per IP during network scans creates a massive process-spawning bottleneck, significantly slowing down the scan and consuming excess system resources.
**Action:** Fetch and cache the entire ARP table once per scan batch, looking up individual IPs from the in-memory Map.

## 2024-05-28 - C++ Network Scanner Threading Bottleneck
**Learning:** Spawning a new `std::thread` per task (e.g., per IP address during a ping sweep or per port during a scan) creates massive overhead and slows down scanning operations significantly, even when throttled by a semaphore/condition variable. The constant context-switching and thread allocation becomes a bottleneck.
**Action:** Always replace per-task thread creation with a fixed-size `std::thread` pool where threads are spawned once, and work is distributed safely by having each thread pull the next task using an `std::atomic<size_t>` index counter.
## 2026-03-08 - Optimize Port Scanning Thread Creation
**Learning:** In C++ network scanners (like `transparency-linux`), spawning a new `std::thread` for every single port being scanned (up to 65k ports) creates massive overhead and memory pressure, even if throttled by a condition variable.
**Action:** Use a fixed-size thread pool (e.g., `std::min(32, (int)ports.size())`) with a shared `std::atomic<size_t>` index to distribute task processing. This turns O(N) thread creations into O(1), significantly improving performance.
## 2025-03-22 - [O(1) Anomaly Lookups in Map Render]
**Learning:** Using an $O(N)$ operation like `Array.prototype.some()` inside a mapping or rendering loop over a large array (like `devices` and `anomalies`) creates a massive performance bottleneck ($O(N \times M)$ complexity).
**Action:** When working on frontends with large rendering scopes, pre-compute lookup tables (`Map` or `Set`) before the rendering loop. Ensure lookups within the loop are $O(1)$ by using `Map.prototype.get()` or `Set.prototype.has()`.
