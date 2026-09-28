# Codespaces Validation Audit

Status: **PARTIAL PASS; REVISED TSAN SCOPE NOT YET EXECUTED**.

## Targets and configurations

The repository now has one root CMake project and these exact presets:

| Purpose | Configure/build/test preset | Compiler | Tests |
|---|---|---|---|
| GCC Release | `gcc-release` | `g++` | all registered single-thread tests plus concurrent correctness |
| Clang Release | `clang-release` | `clang++` | same |
| GCC Debug | `gcc-debug` | `g++` | same |
| Clang Debug | `clang-debug` | `clang++` | same |
| ASan + UBSan | `asan-ubsan` | `clang++` | same, with leak detection supplied at execution |
| TSan | `tsan` | `clang++` | `test_btree_concurrent_correctness` only |

Registered targets are `test_btree`, `test_monitor`, `test_predictor`,
`test_adaptive`, `test_adaptive_regression`, `test_adaptive_aco`,
`test_segmented_adaptive`, `test_v2`, `simple_test`, and
`test_workload_identity`, and `test_btree_concurrent_correctness`. The
workload-identity test replays one seeded operation stream against STATIC,
V1, and V2 with adaptation disabled and checks misses, checksums, sizes, and
final range contents. The concurrent target is now wired into
CMake/CTest and CI. Legacy subdirectory CMake files are incomplete and are not
the Codespaces entry point.

Strict `-Wall -Wextra -Wpedantic -Werror` remains enabled. ASan/UBSan and TSan
are separate. No performance benchmark is a CMake validation target.

Each manual stage creates a new UTC-timestamped evidence directory and records
CPU, OS, compiler, CMake, Ninja, and Git identities under its `environment/`
subdirectory. Test and sanitizer output is retained under `logs/`; the
concurrent stages additionally emit a machine-readable CSV.

## Static findings

- Removed the unused `AccessMonitor::window_size_` field while retaining the
  legacy constructor parameter explicitly as ignored.
- Made V1 replacement-tree construction exception-safe with `unique_ptr`.
- Added quiescent structural validation and separator/leaf-chain checks.
- The original optimistic reader accessed ordinary internal-node fields during
  structural insertion and ordinary values during in-place update. TSan
  confirmed the structural race; version retry does not legalize either race.
- The supported API now assumes an immutable preloaded topology. Its concurrent
  lookup traverses immutable internal nodes and acquires the target leaf lock
  before reading keys or values. Existing-value updates acquire the same lock.
- Concurrent insert/split, remove, clear, bulk load, and adaptive replacement
  remain unsupported and must execute only while workers are quiescent.

## Runtime confirmation required

Environment, GCC, Clang, ASan/UBSan/LSan, and the earlier deterministic
post-join test passed. The revised supported-scope TSan test is `NOT_RUN`.
Do not use historical thread-scaling numbers as evidence for the revised path
until that path passes TSan and is benchmarked again.
