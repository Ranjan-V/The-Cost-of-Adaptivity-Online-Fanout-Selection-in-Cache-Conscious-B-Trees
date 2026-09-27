# Existing-Evidence Completion Report

No benchmark, notebook, cloud job, or upload was performed for this report.
The only executed computation was analysis of existing CSV rows in
`results-now-2.zip`.

## 1. Perfect-run verification result: VERIFIED RESULT

- Raw evidence: 300 Perfect main rows, 300 Perfect phase sidecars, 450 matched
  sample-rate-32 STATIC-REGIONAL/ADAPT-V1/ADAPT-V2 rows, 90 raw static oracle
  calibration rows, and 15 processed per-seed phase oracles.
- Coverage: three regimes x five seeds x ten repetitions x PERFECT-V1/V2.
- Identity: Linux/Kaggle machine `f410f7064c081e00`, Intel Xeon 2.20 GHz,
  four logical CPUs, GCC 11.4.0, source `b9d01f8835b06ae01fb0`, C++11
  `-O3 -march=native -DNDEBUG`, one million records and operations.
- All five-way execution pairs agree on seed, repetition, workload
  fingerprint, checksum, and zero misses. They also agree with the raw oracle
  calibration fingerprint/checksum for the same regime and seed.
- Exact paths and run IDs: `perfect_raw_file_inventory.csv` (750 rows).
- Timed-run `invariant_status` is `NOT_CHECKED`; no invariant result is inferred.

## 2. Perfect decomposition: VERIFIED RESULT

`perfect_decomposition_summary.csv` contains paired hierarchical-bootstrap
95% intervals. Oracle timing has five measurements (one per seed at repetition
zero); its schedule is valid for deterministic repeated streams but is not 50
independent oracle measurements.

| Regime | Oracle/static | Perfect-V1/static | Perfect-V2/static | Adapt-V1/static | Adapt-V2/static |
|---|---:|---:|---:|---:|---:|
| Zero | 1.030 | 0.127 | 0.490 | 0.500 | 0.879 |
| Borderline | 1.021 | 0.223 | 0.625 | 0.531 | 0.885 |
| High | 1.044 | 0.006 | 0.064 | 0.797 | 0.923 |

Positive throughput loss is defined exactly as requested: Oracle minus
Perfect for actuation, and Perfect minus Adaptive for online control. The
online-control loss is negative in every regime: conservative Adaptive V1/V2
outperform their Perfect counterparts by declining expensive rebuilds.

The High regime's very low Perfect ratios are verified. Its 10K-operation
phases create 100 boundaries; the measured schedules cause 470.4 segment
rebuilds per run on average. Observed counts exactly equal fanout transitions
times eight segments. `phase_target()` executes inside global and phase wall
timers. Measured rebuild routines consume 98.5% of V1 and 77.0% of V2 wall
time. These shares are lower bounds on total actuation overhead because they do
not include all surrounding dispatch/control work.

## 3. Memory-analysis status: VERIFIED RESULT WITH MISSING COMPONENTS

See `perfect_memory_summary.csv` and
`../../docs/MEMORY_MEASUREMENT_AUDIT.md`. Peak RSS exists for every matched
row. Tree bytes and bytes/key exist for static and V2. V1 shadow payload has an
8 MB lower-bound estimate. V1 monitor bytes, V1 tree bytes, separate V2 sketch
and heavy-hitter measurements, and complete replacement-tree peak allocation
do not exist and are not fabricated.

## 4. Manuscript corrections: IMPLEMENTED BUT NOT COMPILED

`sigmod_submission/main.tex` now reports the 300 completed Perfect rows,
includes the paired decomposition table, explains the extreme ratios, and
separates ASUS i5-12500H/Windows local evidence from Kaggle Xeon/Linux GCC
11.4 evidence. The manuscript was not compiled in this task.

## 5. Clang/sanitizer diagnosis: IMPLEMENTED BUT NOT EXECUTED

Public CI metadata confirms GCC and paper success and Clang plus ASan/UBSan
failure at the shared build/test step. Anonymous public access does not expose
the complete log. Static inspection identified an unused private
`window_size_` field accepted by GCC but diagnosed by Clang under `-Werror`;
the stale field was removed without weakening warnings or sanitizers. This fix
requires Codespaces/CI validation and must not yet be called successful.

## 6. Concurrent correctness: IMPLEMENTED BUT NOT EXECUTED

`tests/test_btree_concurrent_correctness.cpp` and the quiescent
`validate_invariants()` method prepare deterministic disjoint updates/inserts,
post-join structural checks, and exact reference-model comparison. Passing it
would establish one controlled final-state history, not linearizability.
History capture/checking and TSan are still required; ASan/UBSan do not detect
C++ data races.

## 7. Exact smallest remaining execution plan: RECOMMENDED NEXT EXPERIMENT

1. Codespaces Clang `-Werror`, ASan/UBSan/leak checks, existing correctness,
   prepared concurrent test, and TSan.
2. One matched adaptive scale campaign at 100K, 1M, and 5M.
3. One selected-regime replication on one independent CPU platform.
4. Only for PVLDB: one phase-length campaign around the break-even boundary.

Do not recreate the 1,080 static, 1,620 screening, or 1,740
selected/oracle/Perfect campaigns. Do not rerun Kaggle smoke tests or migrate
Modal work. Preserve all 4,440 substantive Kaggle runs.
