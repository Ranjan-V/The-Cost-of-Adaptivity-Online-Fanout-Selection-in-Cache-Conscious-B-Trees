# ASan, UBSan, and Leak Static Review

Status: **RUNTIME_VALIDATION_REQUIRED**.

## Fixed by inspection

`SegmentedAdaptiveBPlusTree::rebuild_segment` previously held the replacement
tree in a raw pointer while replaying the V1 record map. If allocation or an
insert threw, the replacement leaked. It now uses `std::unique_ptr` and
releases ownership only after replay succeeds. Successful rebuild semantics
are unchanged.

The bulk loader tracks partially constructed nodes and deletes them on failure;
leaf destructors own values, internal nodes do not recursively own children.
V2 also deletes an unsuccessful replacement in its catch path. These paths
still require ASan/LSan confirmation.

## Runtime risks and limitations

- The original optimistic path observed non-atomic node arrays and values while
  writers mutated them. The supported preloaded path now keeps topology
  immutable and locks the target leaf for both value reads and updates. TSan
  confirmation of that revised path remains required.
- `remove()` deletes a value after locking its leaf, while an optimistic reader
  may have observed that value pointer. Concurrent delete is outside the
  prepared test and must not be claimed safe.
- `clear`, `export_sorted`, `bulk_load_sorted`, and adaptive segment replacement
  require quiescence.
- V2 temporary-memory accounting measures the exported vector, not the full
  simultaneous replacement-tree allocation.
- Allocation-failure paths, split boundaries, sibling links, and destructor
  traversal require runtime sanitizer and leak checks.

No suppression file is prepared. Any sanitizer finding is a STOP condition.
