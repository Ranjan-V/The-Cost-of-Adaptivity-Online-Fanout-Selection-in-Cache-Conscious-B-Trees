# When Does Online Fanout Adaptation Pay? Artifact

VALIDATION_STATUS: PARTIAL. GCC, Clang, ASan/UBSan/LSan, and deterministic
post-join concurrent correctness passed in Codespaces. TSan exposed a genuine
race in concurrent structural insertion. The supported concurrency scope is
now restricted to a preloaded, topology-immutable tree with point lookups and
existing-value updates synchronized at the target leaf; its revised TSan gate
must still be rerun. Concurrent insertion, splitting, removal, and adaptive
replacement are unsupported. The 1,080-run static opportunity campaign,
1,620-run dynamic screening campaign, and 1,740-run selected/oracle/Perfect
campaign are archived as `results.zip`, `results-2.zip`, and
`results-now-2.zip`. The 300 Perfect rows have been identity-, fingerprint-,
checksum-, miss-, pairing-, and rebuild-count-verified. Compiler/sanitizer
validation, the 300-run adaptive-scale campaign, the 90-run independent
Windows/laptop replication, and the 525-run phase-length/break-even campaign
are complete and verified. Existing earlier paper CSVs remain separate legacy
results.

This C++11 artifact compares fixed-fanout trees, regional routing, the original segmented adaptive tree (V1), and a bounded-monitor/bulk-load design (V2). The local replication machine is Windows 11, i5-12500H, 16 GB RAM. Kaggle CPU hosts the completed broad sweeps; GPU is unnecessary.

Key directories: `include/` (index and monitors), `benchmarks/` (legacy and unified drivers), `tests/`, `experiments/configs/`, `analysis/`, `scripts/windows/`, `scripts/linux/`, `kaggle/`, `paper_upgrade/`, and `results/`. The precise protocol is in `LOCAL_WINDOWS_RUNBOOK.md`, `kaggle/KAGGLE_RUNBOOK.md`, and `EXECUTION_CHECKLIST.md`.

Windows prerequisites: a current GCC/Clang C++11 compiler and Python 3; matplotlib is needed only to regenerate figures. Preview `scripts/windows/build_release.ps1`, then execute it manually with `-Execute`. Run `scripts/windows/run_correctness.ps1 -Execute` before any campaign. Preview a small campaign with `scripts/windows/run_static_sweep.ps1`; the full grid can be very expensive. Exact runtime is UNKNOWN until measured. The 5M-key configuration can consume substantial RAM because it materializes the tree and operation stream. Check free RAM and avoid swapping. Do not schedule optional 10M keys before a memory audit.

The unified benchmark writes one CSV per completed run under `results/raw/<environment>/<machine>/<family>/`. The campaign runner writes a `.log` and uses a `.partial.csv` before promoting it to a completed result. Repeated invocation skips completed files. Processed summaries and figures are generated only from actual raw CSVs. `analysis/oracle.py` computes a retrospective best-static envelope; it is diagnostic, not a runnable zero-overhead index. `analysis/aggregate.py`, `analysis/paired.py`, and `analysis/plot_maps.py` require real results. See `REPRODUCIBILITY.md` for figure inputs.

Field definitions and measurement caveats are in `experiments/RESULT_SCHEMA.md`.

Kaggle source bundles are execution packages, not evidence by themselves.
Completed claims must be traced to an archived result ZIP and manifest.  The
selected-regime evidence is traced by
`results/manifests/dynamic_selected_kaggle_v2.txt`. Perfect raw-file identity
and derived statistics are recorded in
`results/processed/perfect_raw_file_inventory.csv`,
`perfect_verification.md`, and `perfect_decomposition_summary.csv`. See
`EXECUTION_CHECKLIST.md` before drawing additional scientific conclusions.
