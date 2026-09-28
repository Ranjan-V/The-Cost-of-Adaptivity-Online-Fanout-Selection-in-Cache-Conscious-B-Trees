# Manual Kaggle CPU runbook

STATUS: NOT_RUN. Upload `kaggle_upload.zip` yourself as a Kaggle dataset, then import `kaggle/kaggle_experiments.ipynb` as a notebook. Enable CPU; a GPU is unnecessary. In notebook cell 2, the mounted dataset must contain exactly one `kaggle_upload.zip`. Execute cells one at a time; all cell outputs were intentionally left empty in the distributed notebook.

1. Cell 1: record CPU, RAM, OS, and compiler identity. Save output externally with the campaign.
2. Cells 2-5: unpack, inspect, build, validate. Stop immediately on any error.
3. Cell 6: run a tiny smoke case and inspect the CSV for a nonempty workload fingerprint and zero misses.
4. Cells 7-9: full static grid, measured oracle envelope, then the separate `dynamic_broad` static/regional sweep, measured phase oracle, and representative selection. Cell 9 is deliberately commented: inspect its large grid and available CPU time, then uncomment one command at a time. Each completed run has its own CSV and log. Resuming skips completed files; inspect any `.partial.csv` first.
5. Cells 10-11: replace pending selection with actual selected regimes and then enable one chunk at a time. The template is intentionally blocked until then.
6. Cells 12-13: optional scale and independent cloud replication. Check free RAM before 5M keys. CPU model may change between sessions; never pool raw throughput across different CPU fingerprints.
7. Cells 14-16: aggregate, plot, and create a ZIP for manual download.

Do not treat Kaggle as the controlled source for sub-microsecond latency claims. Preserve raw CSVs, logs, the notebook, and machine identity for each session.

## Adaptive scale campaign

The completed and verified scale campaign is documented in
`docs/ADAPTIVE_SCALE_CAMPAIGN.md`. The instructions below are retained for
artifact reproducibility; do not rerun the campaign for the current evidence.
Package the committed source with `scripts/linux/package_adaptive_scale.sh`,
upload `kaggle_upload_adaptive_scale.zip`, and import
`kaggle/kaggle_adaptive_scale.ipynb`. Use accelerator None. The notebook dry
runs the matrix and requires exactly 300 planned rows before its explicit
execution cell. Its final cell produces `adaptive_scale_results.zip` and
`adaptive_scale_resume.zip`.

## Stage 2: bounded dynamic screening

Use `kaggle/kaggle_dynamic_screening.ipynb` with accelerator set to None. The
notebook runs only `experiments/configs/dynamic/screening.json`; it does not
repeat the completed static opportunity campaign. The grid contains 1,620
paired runs: three phase lengths, three hot fractions, three Zipf parameters,
five seeds, two non-adaptive variants, and six fanouts. Each run executes one
million measured operations after a 10,000-operation warmup.

The final cells derive the zero-cost phase oracle, select measured regimes, and
write both `/kaggle/working/results.zip` and `/kaggle/working/resume.zip`.

## Stage 3: selected adaptive validation

Use `kaggle/kaggle_dynamic_selected.ipynb` with accelerator set to None. It
first measures a 90-run phase-oracle calibration using the same compiled
binary, then runs three regional
regimes selected from paired screening results: zero (0.00%), borderline
(3.00%), and high (9.03%) static-fanout headroom. Adaptive V1/V2 are evaluated
at sampling rates 16, 32, and 64; perfect-detector controls use the measured
rate-32 phase choices. The campaign has 1,740 one-million-operation runs and
ten repetitions per seed. Its final cell writes both result and resume ZIPs.
