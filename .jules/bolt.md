## 2024-03-06 - Optimize ARP table lookups during network scan
**Learning:** In network scanning tools, resolving MAC addresses by executing a shell command (`cat /proc/net/arp` or `arp -n`) per IP address creates a massive bottleneck due to excessive process spawning. Spawning processes concurrently per IP in `Promise.all` slows down the entire system and main thread significantly.
**Action:** Always fetch and cache the entire ARP table once per scan batch instead of querying it per individual IP address. This turns O(N) process executions into O(1), improving performance by >100x for MAC resolution.

# 2026-03-07 - ARP Scanning Bottleneck

**Learning:** Resolving MAC addresses via individual shell commands (`arp -a <ip>`) per IP during network scans creates a massive process-spawning bottleneck, significantly slowing down the scan and consuming excess system resources.
**Action:** Fetch and cache the entire ARP table once per scan batch, looking up individual IPs from the in-memory Map.
