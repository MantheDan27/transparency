## 2025-03-13 - [Debounce Optimization for Search Input]
**Learning:** Adding debouncing to frequent event handlers (like keystrokes) prevents massive synchronous UI blocking in large DOMs (like device tables).
**Action:** When working on frontends with large rendering scopes, implement a debounce wrapper to delay rendering triggers until input stabilizes. Ensure you don't accidentally pull in lockfile dependency changes while doing so.
