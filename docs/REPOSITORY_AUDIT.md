# Static repository audit

AUDIT_STATUS: SOURCE INSPECTION COMPLETE. VALIDATION_STATUS: PARTIAL. The
1,080-row static opportunity, 1,620-row dynamic screening, and 1,740-row
selected/oracle/Perfect Kaggle campaigns are complete. The 300 Perfect rows
are verified and analyzed. Compiler/sanitizer validation and the 300-run
adaptive-scale campaign, the 90-run independent Windows/laptop replication,
and the 525-run phase-length/break-even campaign are also complete.
Hardware counters and ARM are not required by the smallest remaining plan.

## Architecture

- `include/btree/btree.h`: header-only `cabtree::BPlusTree<Key,Value>`. Nodes own dynamically allocated key/pointer arrays and leaf values. `root_` is atomic. Point lookup uses version checks; writes lock nodes. The new `export_sorted()` walks linked leaves under an external quiescence precondition, and `bulk_load_sorted()` builds levels bottom-up from unique sorted records.
- `include/monitor/monitor.h`: legacy `AccessMonitor` uses 64 shards of maps plus sampling, heat score, and decay. Its per-key tracking can grow with distinct keys. V1 retains it.
- `include/predictor/predictor.h`: `FanoutPredictor`, used by segmented V1.
- `include/adaptive/segmented_adaptive_btree.h`: one tree, legacy monitor, predictor, and duplicate `unordered_map` per segment. `insert()` writes both map and tree. `search()` falls back to map. `adapt_segment()` forms heat features, applies votes/cooldown/cost gate. `rebuild_segment()` replays hash-map entries through ordinary inserts. This is ADAPT-V1.
- `include/adaptive/adaptive_btree.h`, `_regression.h`, `_aco.h`: global policy variants retained as historical baselines.
- `include/monitor/bounded_monitor.h` and `include/adaptive/segmented_adaptive_v2.h`: prepared V2. It keeps the tree as source of truth, samples into fixed storage, exports records only after a rebuild decision, and bulk-loads a replacement tree. It is single-threaded at the segmented/adaptation layer.

## Benchmark entry points

- `benchmarks/benchmark_ycsb.cpp`: YCSB-style A/B/C, deterministic generators; legacy CSV at `results/ycsb_summary.csv`.
- `benchmarks/benchmark_shifting.cpp`: phase workloads, static regional configurations and oracle rows; its `OraclePhasedAdapter` also maintains records and rebuilds. Separate rebuild-inclusive throughput appears in legacy output.
- `benchmarks/benchmark_overhead.cpp`: seven-row clean ablation; `scripts/run_benchmark_trials.py` repeats it along with YCSB/shifting.
- `benchmarks/benchmark_wiki_trace.cpp`: reads `data/wiki_pageviews_trace.csv`, or uses a generated fallback; trace provenance must be checked before reporting a real-trace claim.
- `benchmarks/benchmark_threads.cpp`: standalone baseline scalability. This does not establish thread safety of V1/V2.
- `benchmarks/benchmark_dynamic.cpp`: sliding-hotspot workload.
- `benchmarks/benchmark_unified.cpp`: prepared single-threaded V1/V2/static ladder with deterministic workload fingerprint and one CSV per run.

## Results, manuscript, and build

- Existing CSVs in `results/` are legacy current-paper results. Their generators include `scripts/analyze_*.py`, `scripts/generate_paper_results.py`, and plotting scripts. Do not merge them with new campaign rows without matching schema/hardware/flags.
- `sigmod_submission/main.tex` is the current anonymous manuscript source. `paper-final/main.tex` and arXiv copies are distinct older/presentation copies. New experimental claims belong in `paper_upgrade/` until actually measured and reviewed.
- `build.bat` uses C++11, `-O2`, and GCC/MinGW assumptions. Prepared modern builds are in `scripts/windows/build_release.ps1` and `scripts/linux/build_release.sh` with `-O3 -march=native -DNDEBUG` for benchmark only. Compiler version is not assumed.

## Limitations and exact modification sites

- Legacy `BPlusTree::remove()` deletes from the leaf without rebalance; arbitrary deletion/split invariants need further validation. No V2 delete workload is scheduled.
- Existing optimistic search reads mutable node fields without C++ atomic protection of those fields and no epoch/hazard reclamation. It must not be described as proven C++-memory-model-safe concurrent OLC. Multithreaded adaptation is out of scope.
- `BPlusTree::clear()`, `export_sorted()`, and `bulk_load_sorted()` require quiescence. V2 replacement is only used from the single-threaded driver.
- V1's cost scores are heuristic, not measured nanoseconds. V2 uses a related heuristic; this is not an empirical proof of optimal decisions.
- The unified benchmark currently supports read/update only, integer keys/values, and a single thread. It emits run-level and phase-sidecar CSVs, retrospective phase-oracle analysis, and peak-RSS collection where the OS API supports it. Existing Kaggle values are measured; precise V1 tree bytes and full V1 monitoring counters remain UNSUPPORTED.
- Local config paths are repository-relative; runner output is under `results/raw/<environment>/<machine>/<family>/`. Legacy benchmark paths write directly into `results/` and can overwrite their own summary files.
- The selected-regime V2 and Perfect results have passed archive-count,
  failure/partial, fingerprint, checksum, miss, paired-analysis, and Perfect
  rebuild-accounting checks and are included in the paper. No
  other prepared campaign may be inserted until it is built, tested, checked
  for equivalent fingerprints/checksums, and run on the intended machine.

- The adaptive-scale campaign contains 150 exact STATIC/ADAPT-V2 pairs across
  100K, 1M, and 5M records. It passed archive identity, failure/partial,
  workload-fingerprint, checksum, and miss checks and is included in the
  manuscript. Historical oracle rows remain explicitly unpaired context.

- The independent Windows/laptop replication contains 45 exact pairs across
  the same three scales, with zero failures, partials, and misses. Its mean
  ratios remain below one, but only the 1M confidence interval excludes
  parity; 100K and 5M are reported as inconclusive.

- The phase-length campaign contains 150 oracle-grid rows and 375 paid rows,
  forming 125 exact Static-Regional/Perfect-V2/Adaptive-V2 trios.  All have
  zero misses and matched checksums/workload fingerprints.  Zero-cost oracle
  ratios are 1.038--1.087, while Perfect V2 remains at 0.122--0.769 and
  Adaptive V2 at 0.778--0.818 across tested 10K--10M-operation phases.
