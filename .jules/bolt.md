## 2024-05-18 - Avoid Sequential `setDoc` calls in Firebase
**Learning:** Found N+1 performance issues during bulk ingestion of devices via large scan files and desktop sync operations. Sequential `await setDoc` operations in a `for` loop dramatically blocked UI and network panels.
**Action:** Always utilize Firestore's `writeBatch` combined with chunking (up to 500 records per batch) when writing multiple documents concurrently to dramatically speed up ingestion times.
