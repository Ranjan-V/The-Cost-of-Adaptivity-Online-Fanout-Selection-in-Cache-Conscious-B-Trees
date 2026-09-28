# CI and Concurrent-Correctness Status

## Codespaces evidence

GCC 13.3 and Clang 18 passed all 11 registered tests under strict C++11
`-Werror`. ASan/UBSan/LSan also passed all 11 tests. The deterministic
post-join concurrency test passed before TSan was applied.

TSan then exposed a genuine race between `insert_in_node`, which writes an
ordinary internal-node key/pointer array, and `find_leaf_optimistic`, which
reads that array. The version check after the read cannot legalize the earlier
C++ data race. No suppression is used, and concurrent structural insertion is
now explicitly unsupported.

## Supported concurrent validation

`tests/test_btree_concurrent_correctness.cpp` now targets only the paper's
actual scope. The tree is preloaded single-threaded, topology remains immutable,
and workers perform point lookups and updates of existing values. The supported
lookup traverses immutable internal nodes and acquires the target leaf's
version lock before reading leaf keys or values. Updates acquire the same leaf
lock. The test contains read-only, disjoint-update, and mixed read/update
scenarios.

After workers join, the test checks exact size, sorted final state, checksum,
point lookups, range results, structural invariants, and unexpected misses. It
can persist a CSV containing compiler, sanitizer, thread, operation, seed,
count, checksum, miss, invariant, reference, lookup, range, and exit fields.

This is not a linearizability proof. Concurrent insert/split, remove, clear,
bulk loading, and adaptive segment replacement remain unsupported. Historical
thread-scaling numbers used the earlier optimistic value-read path and must be
rerun on the revised supported path before publication.

## Remaining evidence

Run the revised TSan stage and preserve its complete log and CSV. Any race,
deadlock, crash, mismatch, or missing evidence is a STOP condition.
