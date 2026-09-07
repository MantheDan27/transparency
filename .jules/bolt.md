## 2023-10-27 - Firestore N+1 Batching Pattern
**Learning:** In Firebase applications processing bulk client-side telemetry arrays (like devices or scans), sequential `await setDoc` loops create extreme N+1 network request bottlenecks.
**Action:** When performing bulk writes to Firestore, refactor sequential operations to utilize `writeBatch(db)`, buffering up to 500 operations and drastically reducing network overhead.
