## 2024-09-13 - N+1 Firestore Operations
**Learning:** Found sequential `await setDoc()` operations in a loop processing bulk uploads. This causes massive N+1 network request overhead and severely degraded UI performance in Firestore applications for large arrays.
**Action:** Always refactor sequential loop-based Firestore writes into chunked `writeBatch()` operations (up to 500 max per batch) to aggregate network requests and significantly speed up bulk updates.
