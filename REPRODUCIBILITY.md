# Reproducibility map

VALIDATION_STATUS: PARTIAL. GCC 13.3, Clang 18, and ASan/UBSan/LSan passed all
11 registered tests in Codespaces. Deterministic post-join concurrency checks
passed, but TSan exposed a genuine concurrent structural-insert race. The
revised gate covers only the supported preloaded lookup/existing-value-update
scope and remains to be rerun. The three archived Kaggle campaigns are
complete and Perfect decomposition is verified from existing rows. Adaptive
scale and independent-platform replication remain pending.

| Planned figure/table | Raw experiment | Config | Processing |
|---|---|---|---|
| Static opportunity map | STATIC fanout grid | `experiments/configs/static_opportunity/grid.json` | `analysis/oracle.py`, `analysis/plot_maps.py` |
| Break-even map | Selected STATIC/V1/V2 pairs | selected config after measured regime selection | `analysis/paired.py --output ...`, `analysis/plot_maps.py --paired ...`; CI classification only |
| Phase-length curve | Regional static phase grid, then shifting phase lengths | `experiments/configs/phase_length_oracle_grid.json`, then `experiments/configs/phase_length.json` | `analysis/phase_oracle.py`, `analysis/aggregate.py`, `analysis/plot_campaign.py --phase-oracle ...` |
| Monitor ladder | STATIC observation variants, V1/V2 | `experiments/configs/monitor_ladder.json` | `analysis/aggregate.py`, `analysis/paired.py`, `analysis/plot_campaign.py` |
| Rebuild decomposition | Perfect V1/V2 vs static/oracle | completed in `results-now-2.zip` | `analysis/perfect_decomposition.py`; outputs under `results/processed/perfect_*`; real rebuild time charged |
| Scale curve | 100K, 1M, 5M | `experiments/configs/scale.json` | `analysis/aggregate.py`, `analysis/plot_campaign.py` |
| Cloud replication | repeated selected configs on separate CPU group | selected config | normalized paired analysis only |

Legacy paper figures under `sigmod_submission/figures/` and `paper-final/figures/` come from earlier CSVs/scripts. Keep them separate from the new V2 campaign until revalidation.

The zero-cost envelope is a retrospective choice among measured whole-regional static layouts, not a deployable index or a claim of a universal upper bound. The executable perfect detector applies the selected fanout to all regions and charges actual rebuild time. Its oracle CSV must match machine, source, compiler flags, workload, seed and sample-rate stratum; one measured oracle repetition may supply choices to repeated runs of the same deterministic operation stream. `plot_maps.py` writes one figure per homogeneous stratum, with a short hash suffix in the filename.
