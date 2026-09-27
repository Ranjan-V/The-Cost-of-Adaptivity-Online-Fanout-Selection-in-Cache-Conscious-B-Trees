# When Does Online Fanout Adaptation Pay? Artifact

VALIDATION_STATUS: PARTIAL. The 1,740-run selected-regime V2 campaign is
complete and archived as `results-now-2.zip`; the broader multi-platform V2,
perfect-detector, scale, compiler, and cross-platform campaigns remain
RESULT_PENDING. Existing earlier paper CSVs remain separate legacy results.

This C++11 artifact compares fixed-fanout trees, regional routing, the original segmented adaptive tree (V1), and a bounded-monitor/bulk-load design (V2). The controlled machine is Windows 11, i5-12500H, 16 GB RAM. Kaggle CPU is for broad sweeps and independent replication; GPU is unnecessary.

Key directories: `include/` (index and monitors), `benchmarks/` (legacy and unified drivers), `tests/`, `experiments/configs/`, `analysis/`, `scripts/windows/`, `scripts/linux/`, `kaggle/`, `paper_upgrade/`, and `results/`. The precise protocol is in `LOCAL_WINDOWS_RUNBOOK.md`, `kaggle/KAGGLE_RUNBOOK.md`, and `EXECUTION_CHECKLIST.md`.

Windows prerequisites: a current GCC/Clang C++11 compiler and Python 3; matplotlib is needed only to regenerate figures. Preview `scripts/windows/build_release.ps1`, then execute it manually with `-Execute`. Run `scripts/windows/run_correctness.ps1 -Execute` before any campaign. Preview a small campaign with `scripts/windows/run_static_sweep.ps1`; the full grid can be very expensive. Exact runtime is UNKNOWN until measured. The 5M-key configuration can consume substantial RAM because it materializes the tree and operation stream. Check free RAM and avoid swapping. Do not schedule optional 10M keys before a memory audit.

The unified benchmark writes one CSV per completed run under `results/raw/<environment>/<machine>/<family>/`. The campaign runner writes a `.log` and uses a `.partial.csv` before promoting it to a completed result. Repeated invocation skips completed files. Processed summaries and figures are generated only from actual raw CSVs. `analysis/oracle.py` computes a retrospective best-static envelope; it is diagnostic, not a runnable zero-overhead index. `analysis/aggregate.py`, `analysis/paired.py`, and `analysis/plot_maps.py` require real results. See `REPRODUCIBILITY.md` for figure inputs.

Field definitions and measurement caveats are in `experiments/RESULT_SCHEMA.md`.

Kaggle source bundles are execution packages, not evidence by themselves.
Completed claims must be traced to an archived result ZIP and manifest.  The
selected-regime evidence is traced by
`results/manifests/dynamic_selected_kaggle_v2.txt`; all other new campaign
claims remain pending. See `EXECUTION_CHECKLIST.md` before drawing additional
scientific conclusions.
