1. **Journal the Performance Finding (Bolt persona)**
   - Create/append to `.jules/bolt.md` documenting the N+1 issue found in Firestore `setDoc` calls within loops (e.g., in upload/sync functions), highlighting that this causes UI blocking on large dataset ingestion. Include how to prevent it using `writeBatch()`.

2. **Add `writeBatch` to Firestore Imports**
   - Modify `transparency-web/public/js/dashboard.js` using a Python script to import `writeBatch` from Firebase Firestore.

3. **Optimize the `uploadForm` submit handler**
   - Modify the `uploadForm.addEventListener("submit", ...)` loop in `transparency-web/public/js/dashboard.js` to use `writeBatch()`. Instead of awaiting `setDoc` for every device, we will initialize a batch, queue the writes, and await `batch.commit()` at the end, executing in chunks of 500 if necessary (or simply all at once if dataset size is implicitly expected to be small, but a chunking logic is safer for robust performance optimization).
   - *Wait*, a 500-limit check is best practice. Let's implement chunked batching.

4. **Optimize the `syncDevicesBtn` click handler**
   - Similarly, modify the loop in the sync devices function in `transparency-web/public/js/dashboard.js` to use `writeBatch()` to prevent N+1 queries.

5. **Verify Changes**
   - Provide a Python static verification script (`verify.py`) to assert that the `await setDoc` calls within the loops are replaced with `batch.set`, and `batch.commit()` is present, confirming the optimization.
   - Serve the static files and log an `echo "No automated tests found..."` per constraints, since `transparency-web` lacks them.

6. **Cleanup Temporary Files**
   - Run `rm -f plan.md match.py test_batch.py modify.py verify.py`.

7. **Complete pre-commit steps**
   - Complete pre-commit steps to ensure proper testing, verification, review, and reflection are done.
