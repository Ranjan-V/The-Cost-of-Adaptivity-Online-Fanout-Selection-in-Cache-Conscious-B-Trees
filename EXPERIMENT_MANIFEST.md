# Experiment manifest

VALIDATION_STATUS: PARTIAL. Local unit, structural, and paired semantic smoke tests passed. The 1,740-run Kaggle selected-regime campaign completed; Modal overflow and final controlled laptop campaigns remain pending.

- Primary controlled environment: ASUS TUF F15, i5-12500H, 16 GB, Windows 11; actual CPU/core/clock facts must be captured during execution.
- Cloud environment: Kaggle CPU; each session gets its own machine fingerprint and provenance.
- New binary: `benchmark_unified.cpp`, C++11; output: one completed CSV per run plus log.
- Pairing identity: machine fingerprint, experiment family, parameter tuple, seed, repetition. A workload fingerprint is logged and must agree within pairs.
- Static grid: 3 record counts, 4 Zipf values, 3 read/update mixes, 6 fanouts, 5 seeds; 1080 configurations before repetitions. Full execution cost is UNKNOWN.
- Dynamic broad grid is diagnostic/static only. Selected V1/V2 regimes were populated from measured screening results; the selection and validation artifacts are retained separately.
- Long phase grid: 10K, 100K, 1M, 5M, 10M operations per phase. The matching `phase_length_oracle_grid.json` must run first, followed by `analysis/phase_oracle.py`; some phases may be truncated by total operation count, so inspect actual phase counts.
- Adaptive default fanout 64, 8 segments, interval 5000, sample rate 1/32. Sweep sampling 1/8 through 1/128.
- Oracle status: `analysis/oracle.py` computes a retrospective measured best-static envelope; `analysis/phase_oracle.py` prepares a per-phase retrospective envelope from static-regional sidecars. `PERFECT-V1`/`PERFECT-V2` pay measured whole-regional rebuild costs. Claims require completed execution and pairing checks. The envelope is optimistic among tested fanouts, not a mathematical upper bound on every possible layout.
- Compiler robustness: `experiments/configs/compiler_robustness.json` is a small paired subset; run separately with GCC and Clang only after each build passes correctness. Keep compiler groups distinct.
