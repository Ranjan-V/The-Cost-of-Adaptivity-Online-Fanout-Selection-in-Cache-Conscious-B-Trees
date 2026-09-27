# Perfect Campaign Verification

VERIFIED against `results-now-2.zip` without running benchmarks.

- 300 Perfect rows: 3 regimes x 5 seeds x 10 repetitions x 2 variants.
- 450 matched selected rows at sample rate 32: STATIC-REGIONAL, ADAPT-V1, ADAPT-V2.
- 90 raw oracle calibration rows and 15 processed per-seed phase oracles.
- Every five-way execution pair matches seed, repetition, workload fingerprint, checksum, and zero misses; fingerprints/checksums also match the corresponding raw oracle calibration stream.
- Every Perfect rebuild count equals fanout transitions in its measured phase-oracle schedule times eight segments.
- Timed rows report `NOT_CHECKED`; invariant evidence is separate and is not inferred here.
- Oracle timing is measured only at repetition 0 per seed. Reusing its deterministic schedule for repetitions 1-9 is workload pairing, not an independent timing replicate.

## Extreme high-opportunity regime

| Variant | Mean ops/s | Median ops/s | Mean vs static | 95% CI ratio | Mean rebuilds | Rebuild routine share |
|---|---:|---:|---:|---:|---:|---:|
| STATIC-REGIONAL | 3746006 | 3743949 | 1.0000 | [1.0000, 1.0000] | -- | -- |
| ORACLE-ZERO-COST | 3909285 | 3909562 | 1.0436 | [1.0392, 1.0491] | -- | -- |
| PERFECT-V1 | 23646 | 22565 | 0.0063 | [0.0058, 0.0069] | 470.4 | 98.5% |
| PERFECT-V2 | 238931 | 229938 | 0.0638 | [0.0585, 0.0707] | 470.4 | 77.0% |
| ADAPT-V1 | 2983978 | 2984388 | 0.7966 | [0.7942, 0.7991] | 0.0 | 0.0% |
| ADAPT-V2 | 3458860 | 3467645 | 0.9234 | [0.9157, 0.9289] | 0.0 | 0.0% |

The phase length is 10,000 operations (100 phases). Perfect applies each measured target to all eight segments and charges those rebuilds inside `wall_seconds`. The extreme ratio is therefore an observed actuation cost, not a predictor error. `rebuild_total_ms/wall_seconds` reports only timed rebuild routines and is a lower bound on full actuation/control overhead.
