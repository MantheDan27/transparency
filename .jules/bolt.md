## $(date -u +%Y-%m-%d) - Array Filter Optimization
**Learning:** Found multiple separate `Array.prototype.filter()` and loop passes iterating over the same dataset for filtering and counting side-effects. This caused unnecessary `O(N)` passes and memory allocations.
**Action:** Combine multiple array traversals into a single `for...of` loop where possible, preserving `O(1)` reference assignments for default/unfiltered cases. Ensure fast string matching like `Array.includes()` is used instead of instantiating new `Set` objects for small fixed lists of options inside hot functions.
