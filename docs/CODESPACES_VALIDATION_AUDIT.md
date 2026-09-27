# Codespaces Validation Audit

Status: **IMPLEMENTED, NOT EXECUTED**.

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
- The OLC reader accesses ordinary node fields concurrently with mutation.
  Version validation may provide algorithmic retry behavior, but it does not
  by itself prove freedom from C++ data races. TSan runtime confirmation is
  mandatory before stronger wording.

## Runtime confirmation required

Every preset, sanitizer result, concurrent outcome, compiler diagnostic, and
devcontainer package availability remains `NOT_RUN`. Do not start scientific
campaigns until `docs/VALIDATION_GATE.md` passes.
