# Manual execution checklist

Status is per stage. Local V2 correctness, Kaggle dynamic screening/selected
validation, adaptive scale, the reduced independent laptop replication, and
the phase-length campaign are complete and verified.
Stop after any
correctness failure or unexplained data mismatch.

| Stage | Run manually | Expected output | Inspect / stop condition |
|---|---|---|---|
| A tiny validation | Build script and `run_correctness.ps1 -Execute` | binaries, test log | Stop on compile/test failure; do not infer success from this document. |
| B local smoke | Unified binary, 1K keys/10K ops, paired static/V2 | two raw CSVs/logs | Match workload fingerprints and read checksums, zero misses. |
| C Kaggle static opportunity | notebook cells 1-8, or Linux campaign | per-run raw CSVs, oracle CSV | Check CPU fingerprint, partial files, completed seed coverage. |
| D dynamic static sweep | `experiments/configs/dynamic/broad.json` on Kaggle CPU, in chunks | static/regional raw CSVs and phase sidecars | Large matrix; inspect RAM, checkpoint per run, and do not pool CPU models. |
| E representative regimes | `experiments/select_regimes.py --family dynamic_broad` | selected JSON | Require measured same-stratum static pairs; do not choose by desired outcome. Populate selected config only from these results. |
| F phase oracle | `analysis/phase_oracle.py` on matched static-regional sidecars | `results/processed/phase_oracle.csv` | Check f64 and another fanout for each phase; source/machine/seed/sample-rate must match before perfect variants. |
| F2 Modal overflow | `modal/modal_campaign.py`, two-job smoke before full map | distinct CSV/log/phase files in Volume v2 | CPU only; group by machine fingerprint; do not use for final microarchitectural timing. |
| G local rigorous V1/V2 | Windows selected/phase/monitor scripts | paired raw CSVs/logs | Use 10-15 repetitions where practical, randomized order, stable power. Run phase-length regional oracle grid and derive its phase oracle before phase-length perfect variants. |
| I Kaggle replication | notebook cells 10-13 | separate CPU-group raw CSVs | Do not pool distinct cloud CPU models. |
| J scale | Completed Kaggle adaptive-scale campaign | 300 verified rows at 100K/1M/5M | Preserve the completed archives; do not rerun. |
| J2 independent scale replication | Complete: Windows laptop cohort | 90 rows, 45 verified exact pairs, summary/figure/archive | Means are below parity; only the 1M paired CI excludes parity. |
| J3 phase length | Complete: Kaggle CPU cohort | 525 rows, 125 exact decomposition trios, summary/figure/archives | Preserve evidence; no paid or online parity through tested 10M-operation phases. |
| K analysis | `analysis/validate_pairs.py`, `aggregate.py`, `paired.py --output`, `oracle.py`, `phase_oracle.py`, plots | processed CSVs/PDFs | Examine CV, n, paired CI, checksum equality. |
| L paper update | integrate measured tables/figures | revised manuscript | No result without raw output and reproducible provenance. |
| M venue decision | `paper_upgrade/VLDB_VS_ICDE_DECISION_RULE.md` | decision memo | Decide only after data quality audit. |

Failed run handling: preserve its log and `.partial.csv`; record reason; never convert to zero or quietly exclude. Resume skips completed files. The binary and campaign scripts must first be manually validated because this preparation was not executed.
