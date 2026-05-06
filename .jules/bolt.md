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
## 2026-10-24 - C++ Wait Loop Performance Optimization
**Learning:** Using busy-wait loops with `Sleep()` inside a background worker thread (`Monitor::WorkerLoop`) consumes unnecessary CPU cycles and causes lag in responsiveness to stop/update requests, as the thread wakes up continuously just to check flags.
**Action:** Replace `Sleep()`-based busy-wait loops with `std::condition_variable::wait_for` alongside a `std::unique_lock`. This allows the thread to sleep optimally without consuming CPU, while remaining instantly responsive to `notify_all()` when configuration changes or the application shuts down.
## 2026-10-25 - [O(1) Anomaly Lookups in UI Bulk Operations]
**Learning:** Using an $O(N)$ operation like `Array.prototype.find()` or `Array.prototype.includes()` inside a loop over selected devices creates a significant performance bottleneck ($O(N \times M)$ complexity) when applying bulk actions on large device lists. This can cause the UI to freeze or become unresponsive.
**Action:** When implementing bulk operations on large data sets (like devices or tables), pre-compute a lookup table (e.g. `Map` or `Set`) before the loop. Replace $O(N)$ lookups with $O(1)$ operations (`Map.prototype.get()` or `Set.prototype.has()`) to reduce the complexity to $O(N + M)$ and maintain a smooth user experience.
## 2024-05-30 - N+1 IPC Bottleneck in Electron Bulk Actions
**Learning:** Iterating through a large array and calling an asynchronous IPC method (`ipcRenderer.invoke`) per item creates an N+1 performance bottleneck due to excessive IPC overhead, context switching, and potential synchronous disk I/O in the main process.
**Action:** Always batch related IPC updates into a single "bulk" method (e.g., `bulkDeleteLocalDevices`) passing the array of identifiers, turning O(N) IPC calls into O(1). Filter the array in the main process using an O(1) Set lookup.
## 2026-10-25 - [Consolidating Chained Array Filters in UI Renders]
**Learning:** Chaining multiple `.filter().length` calls on a large array (e.g., `allDevices.filter(...).length`) to compute various UI statistics creates redundant $O(N)$ passes and unnecessary intermediate array allocations, significantly degrading performance during frequent render cycles.
**Action:** Consolidate multiple filter counting operations into a single `for...of` loop over the array with independent accumulator variables, turning $O(K \times N)$ time complexity into a single $O(N)$ pass.
