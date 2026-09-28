# The Cost of Adaptivity: Online Fanout Selection in Cache-Conscious B-Trees

[![Reproducibility](https://github.com/Ranjan-V/The-Cost-of-Adaptivity-Online-Fanout-Selection-in-Cache-Conscious-B-Trees/actions/workflows/reproducibility.yml/badge.svg)](https://github.com/Ranjan-V/The-Cost-of-Adaptivity-Online-Fanout-Selection-in-Cache-Conscious-B-Trees/actions/workflows/reproducibility.yml)

This repository is a SIGMOD/VLDB-style systems research prototype for studying
whether B+ trees should adapt node fanout online based on observed access
patterns.

The original hypothesis was optimistic: hot regions should use small fanout for
cache locality, while cold regions should use larger fanout for height reduction.
The current evidence is more interesting and more publishable as a diagnostic
systems result:

> Online fanout adaptation has real oracle headroom on shifting workloads, but
> the measured costs of monitoring, records metadata, policy checks, and rebuilds
> often exceed that headroom on in-memory workloads.

The project is therefore positioned as a principled positive/negative study:
it builds adaptive indexes, measures where they fail, adds oracle baselines, and
derives a break-even model explaining when adaptivity can and cannot win.

## Current Status

Implemented and tested:

- Research-prototype C++11 B+ tree with configurable fanout.
- Sampled access monitor with heat scores.
- Fanout predictor with exponential smoothing.
- Global adaptive B+ tree variants using reinforcement learning, regression,
  and ant-colony-style policies.
- Segmented adaptive B+ tree where each key range owns its own B+ tree,
  monitor, records metadata, and adaptation state.
- YCSB-style workloads A/B/C.
- Shifting hot-region benchmark with oracle fanout rows.
- Wikipedia pageview trace benchmark.
- Final clean overhead ablation.
- A preloaded-topology concurrent mode in which point readers and existing-value
  updaters synchronize on per-leaf version locks. Concurrent structural
  insert, split, merge, deletion, and adaptive replacement are unsupported.
- Sliding-hotspot dynamic benchmark where the hot 20% key range moves every
  one million operations.
- Paper-ready algorithm blocks for optimistic lookup and the cost-aware rebuild
  gate.
- Mathematical break-even analysis for a negative-result paper.

The code targets C++11 and GCC 6.3.0 on Windows/MinGW.

## Experimental Upgrade Status

Adaptive V2 removes V1's duplicate shadow-record map, uses a fixed-size sampled
monitor, and bulk-loads replacement segments from sorted records.  Local GCC
correctness tests and paired STATIC/V1/V2 smoke checks pass with identical
checksums and workload fingerprints and zero misses.  The completed Kaggle
selected-regime campaign contains 1,740 runs; its archive provenance and
processed summaries are recorded under `results/manifests/` and
`results/processed/`.

Broader Modal screening, the final controlled laptop campaign, hardware-counter
analysis, and ARM/independent-x86 replication remain pending.  Prepared code is
not evidence of a completed experiment.  Start with
[ARTIFACT_README.md](ARTIFACT_README.md),
[EXECUTION_CHECKLIST.md](EXECUTION_CHECKLIST.md), and
[docs/REPOSITORY_AUDIT.md](docs/REPOSITORY_AUDIT.md).

## Repository Layout

```text
include/
  btree/btree.h                         Baseline B+ tree
  monitor/monitor.h                     Sampled access monitor
  predictor/predictor.h                 Fanout predictor
  adaptive/adaptive_btree*.h            Global adaptive variants
  adaptive/segmented_adaptive_btree.h   Per-range adaptive implementation
  adaptive/segmented_adaptive_v2.h      Bounded-monitor/bulk-load V2
  monitor/bounded_monitor.h             Fixed-space sampled monitor

tests/
  test_btree.cpp
  test_monitor.cpp
  test_predictor.cpp
  test_adaptive*.cpp
  test_segmented_adaptive.cpp
  test_v2.cpp

benchmarks/
  benchmark_ycsb.cpp                    Stationary YCSB-style workloads
  benchmark_shifting.cpp                Shifting hot-region + oracle benchmark
  benchmark_overhead.cpp                Final clean ablation
  benchmark_wiki_trace.cpp              Real Wikipedia pageview trace
  benchmark_threads.cpp                 Preloaded lookup/update scalability
  benchmark_dynamic.cpp                 Sliding hotspot stress test
  benchmark_unified.cpp                 Reproducible STATIC/V1/V2 driver

scripts/
  analyze_ycsb.py
  analyze_shifting.py
  analyze_overhead.py
  analyze_wiki_trace.py
  run_benchmark_trials.py
  generate_paper_results.py
  fetch_wiki_pageviews.py

math/
  mathproofs.tex                        Break-even theorem and calculations
  mathproof.tex                         Compatibility wrapper

paper/tables/
  Auto-generated LaTeX result tables

results/
  Curated CSV summaries, manifests, provenance, and figures

modal/
  CPU-only overflow-screening plan and dry-run-first launcher
```

## Build

Create and activate the Python environment locally. The `venv/` directory is
intentionally ignored by Git and should not be uploaded:

```powershell
py -3 -m venv venv
.\venv\Scripts\python.exe -m pip install -r requirements.txt
```

Activate the environment if needed:

```powershell
.\venv\Scripts\Activate.ps1
```

Build everything:

```powershell
.\build.bat all
```

Build only the final overhead ablation:

```powershell
.\build.bat overhead
```

## Tests

Run the core correctness tests:

```powershell
.\build\test_btree.exe
.\build\test_monitor.exe
.\build\test_predictor.exe
.\build\test_segmented_adaptive.exe
```

Run all adaptive policy tests:

```powershell
.\build\test_adaptive.exe
.\build\test_adaptive_regression.exe
.\build\test_adaptive_aco.exe
```

## Benchmarks

Stationary YCSB-style workloads:

```powershell
.\build\bench_ycsb.exe 100000 100000 0.99 5000
.\venv\Scripts\python.exe scripts\analyze_ycsb.py
```

Shifting hot-region workload with oracle baselines:

```powershell
.\build\bench_shifting.exe 100000 30000 0.99 5000
.\venv\Scripts\python.exe scripts\analyze_shifting.py
```

Final clean overhead ablation:

```powershell
.\build\bench_overhead.exe 100000 100000 0.99 5000
.\venv\Scripts\python.exe scripts\analyze_overhead.py
```

Wikipedia pageview trace:

```powershell
.\venv\Scripts\python.exe scripts\fetch_wiki_pageviews.py --days 10 --articles-per-day 1000 --output data\wiki_pageviews_trace.csv
.\build\bench_wiki_trace.exe data\wiki_pageviews_trace.csv 30000 5000 10000 0.02
.\venv\Scripts\python.exe scripts\analyze_wiki_trace.py
```

Preloaded lookup/update scalability (rerun required after the concurrency
correctness fix; historical throughput is not correctness evidence):

```powershell
.\build\bench_threads.exe 1000000 10000000 0.95
```

Sliding hotspot benchmark:

```powershell
.\build\bench_dynamic.exe 1000000 5000000 1000000 0.99 5000
```

Repeat YCSB and shifting trials:

```powershell
.\venv\Scripts\python.exe scripts\run_benchmark_trials.py 5 100000 100000 0.99 5000 30000
```

The trial runner now repeats YCSB, shifting, and the final clean overhead
ablation. It writes:

- `results/ycsb_trials.csv`
- `results/ycsb_trial_summary.csv`
- `results/shifting_trials.csv`
- `results/shifting_trial_summary.csv`
- `results/overhead_trials.csv`
- `results/overhead_trial_summary.csv`

Optional seventh argument controls overhead operations if you want it different
from YCSB:

```powershell
.\venv\Scripts\python.exe scripts\run_benchmark_trials.py 5 100000 100000 0.99 5000 30000 100000
```

Generate paper-ready tables and the result summary:

```powershell
.\venv\Scripts\python.exe scripts\generate_paper_results.py
```

Generate final paper figures. Each figure is written twice: once under
`paper-final/figures/` for the LaTeX source, and once under `results/figures/`
for easy inspection.

```powershell
.\venv\Scripts\python.exe scripts\generate_final_paper_figures.py
```

Generated artifacts:

- `results/research_results_summary.md`
- `paper/tables/ycsb_summary.tex`
- `paper/tables/shifting_summary.tex`
- `paper/tables/overhead_ablation.tex`
- `paper/tables/wiki_trace_summary.tex`

## GitHub Reproducibility Notes

The repository is configured to keep the research evidence and paper inputs in
Git while excluding local build products. In particular, the following should be
committed:

- Source code under `include/`, `benchmarks/`, `tests/`, `scripts/`, and `math/`.
- Benchmark evidence under `results/`, including CSV summaries and generated
  figure PDFs.
- The compact Wikipedia trace in `data/wiki_pageviews_trace.csv`.
- Paper sources under `paper/` and `paper-final/`, including ACM style files,
  bibliography files, and generated figures.

The following should remain local and are ignored by `.gitignore`:

- `venv/`
- `build/`
- compiled executables and object files
- LaTeX intermediate files such as `.aux`, `.log`, and `.bbl`
- downloaded third-party source trees such as `integration/postgresql/postgresql-*`

## Latest Key Results

Five-trial YCSB summary at 100K records and 100K operations/workload:

| Workload | Static f64 | Segmented Adaptive | Adaptive / Static |
|---|---:|---:|---:|
| YCSB-A | 5.05M ops/s | 4.08M ops/s | 0.81x |
| YCSB-B | 7.30M ops/s | 5.07M ops/s | 0.69x |
| YCSB-C | 7.56M ops/s | 5.34M ops/s | 0.71x |

Five-trial shifting hot-region aggregate:

| Index | Throughput | vs Static f64 |
|---|---:|---:|
| Oracle Hot f32 | 6.04M ops/s | 1.08x |
| Static B+Tree f64 | 5.59M ops/s | 1.00x |
| Segmented Adaptive | 4.90M ops/s | 0.88x |

Recent Wikipedia pageview trace:

| Index | Throughput | vs Static f64 |
|---|---:|---:|
| Static B+Tree f256 | 10.42M ops/s | 1.00x |
| Static B+Tree f64 | 10.39M ops/s | 1.00x |
| Segmented Adaptive | 8.40M ops/s | 0.81x |

The historical thread-scaling table was generated by the superseded optimistic
value-read path and is not retained as validated evidence.  The revised
preloaded lookup/update path must pass TSan and be measured again before new
scalability numbers are reported.

Sliding hotspot benchmark with 1M keys and five 1M-operation windows:

| Index | Throughput | vs Static f64 |
|---|---:|---:|
| Static B+Tree f64 | 4.18M ops/s | 1.00x |
| Static Region x8 | 3.68M ops/s | 0.88x |
| Segmented Adaptive | 2.62M ops/s | 0.63x |

Final clean overhead ablation rows:

1. Static B+Tree
2. Static Region
3. Static Region + Monitor
4. Segmented No Records
5. Segmented Records Only
6. Segmented Monitor + Records
7. Segmented Full Adaptive

This ablation isolates routing, monitoring, records metadata, and adaptive
decision/rebuild costs. The latest single-run output is stored in
`results/overhead_summary.csv`; the analysis is printed by
`scripts/analyze_overhead.py`.

## Mathematical Result

The paper model is in `math/mathproofs.tex`.

The central break-even condition is:

```text
T * (fanout_benefit - monitoring - records - policy) > rebuild_cost
```

Equivalently, adaptation is impossible to justify for a phase if:

```text
fanout_benefit <= monitoring + records + policy
```

Measured examples:

- YCSB-A adaptive penalty: about 0.047 us/op.
- YCSB-B adaptive penalty: about 0.060 us/op.
- YCSB-C adaptive penalty: about 0.055 us/op.
- Shifting oracle benefit: about 0.013 us/op, while adaptive penalty is about
  0.025 us/op.
- Wikipedia trace fanout-selection benefit is about 0.0003 us/op, while
  adaptive penalty is about 0.023 us/op.

These values explain why the current adaptive implementation loses despite
having a meaningful oracle benchmark.

## Paper Direction

Recommended title:

**The Cost of Adaptivity: Online Fanout Selection in Cache-Conscious B-Trees**

Recommended thesis:

> Online fanout adaptation is not free. On realistic in-memory workloads, the
> oracle benefit of changing fanout can be smaller than the cost of observing,
> deciding, and rebuilding. A cache-adaptive B-tree must therefore be evaluated
> with oracle headroom and overhead ablations, not only with adaptive-vs-static
> throughput.

This is currently stronger as a SIGMOD/VLDB negative-result or diagnostic
systems paper than as a positive "2-3x faster" paper. The work is still valuable
because it provides implementation evidence, oracle baselines, real-trace data,
and a mathematical explanation for the failure mode.

### Selected-regime Kaggle validation

The completed Stage 3 CPU campaign contains 1,740 one-million-operation runs
with no failed or partial outputs. Seed/repetition-matched hierarchical
bootstrap analysis finds that the best Adaptive V2 configuration reaches
0.881x, 0.890x, and 0.931x of static-regional throughput in the zero,
borderline, and high-headroom regimes. The corresponding retrospective
zero-cost oracle ratios are 1.032x, 1.029x, and 1.041x. Even in the strongest
regime, the measured adaptive penalty (0.0199 us/op) exceeds the oracle benefit
(0.0106 us/op).

Reproduce the processed tables and figures directly from the downloaded
archive with:

```powershell
.\venv\Scripts\python.exe analysis\dynamic_selected_report.py `
  --archive results-now-2.zip `
  --processed-dir results\processed `
  --table paper\tables\dynamic_selected_validation.tex `
  --figure-dir results\figures `
  --figure-dir paper-final\figures
```
