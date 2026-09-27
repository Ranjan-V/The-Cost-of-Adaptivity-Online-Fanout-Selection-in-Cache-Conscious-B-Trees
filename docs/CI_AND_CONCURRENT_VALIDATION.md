# CI and Concurrent-Correctness Status

## Current CI diagnosis

The public GitHub Actions run for commit `4a494ef` shows GCC and manuscript
success, with both Clang and ASan/UBSan failing in the shared Clang build/test
step. Public annotations expose only exit code 1, not the compiler log. Static
inspection found an initialized but semantically unused private
`AccessMonitor::window_size_` field. Clang diagnoses unused private fields under
the workflow's `-Wall -Wextra -Werror`; GCC does not diagnose this field under
the same switches. The field has been removed while the constructor parameter
is retained and explicitly documented as a legacy, ignored argument.

This is a prepared diagnosis, not a successful rerun. The next Codespaces/CI
validation must preserve `-Werror`, ASan, UBSan, and leak detection. If the
Clang job still fails, retain and inspect the complete compiler log before any
additional change. Sanitizer checks must not be disabled or converted to
warnings.

## Prepared concurrent validation

`tests/test_btree_concurrent_correctness.cpp` is a deterministic controlled
test with four synchronized writers. Writers own disjoint update and insertion
partitions, then join. The quiescent final tree is checked against a reference
model using size, point lookup, range cardinality, sorted export, parent links,
node capacities, uniform leaf depth, and the leaf chain. An optional CSV path
persists the outcome.

This test establishes deterministic final-state agreement and structural
invariants for one controlled concurrent history. It is **not** a
linearizability proof. In particular, it does not reconstruct invocation and
response intervals, enumerate legal sequential histories, exercise arbitrary
same-key conflicts, or validate concurrent segment replacement. The current
version-counter reader also accesses ordinary node fields while writers can
mutate them; ASan/UBSan do not detect C++ data races. A ThreadSanitizer run and
a history-based checker are required before making a linearizability claim.

## Execution evidence to persist next

1. Clang C++11 build log with `-Werror`.
2. ASan/UBSan and leak-detection logs with no suppressed findings.
3. Concurrent test CSV plus full stdout/stderr.
4. Existing single-thread structural tests and semantic smoke output.
5. A separate TSan result, or an explicit manuscript limitation if TSan is not
   clean; do not describe OLC throughput alone as correctness evidence.
