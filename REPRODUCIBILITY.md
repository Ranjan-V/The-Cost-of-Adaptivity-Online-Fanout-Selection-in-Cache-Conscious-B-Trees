# Reproducibility map

VALIDATION_STATUS: PARTIAL. Local correctness and the selected-regime Kaggle campaign are complete. Rows marked by pending configs below require their remaining campaigns before use in a submission.

| Planned figure/table | Raw experiment | Config | Processing |
|---|---|---|---|
| Static opportunity map | STATIC fanout grid | `experiments/configs/static_opportunity/grid.json` | `analysis/oracle.py`, `analysis/plot_maps.py` |
| Break-even map | Selected STATIC/V1/V2 pairs | selected config after measured regime selection | `analysis/paired.py --output ...`, `analysis/plot_maps.py --paired ...`; CI classification only |
| Phase-length curve | Regional static phase grid, then shifting phase lengths | `experiments/configs/phase_length_oracle_grid.json`, then `experiments/configs/phase_length.json` | `analysis/phase_oracle.py`, `analysis/aggregate.py`, `analysis/plot_campaign.py --phase-oracle ...` |
| Monitor ladder | STATIC observation variants, V1/V2 | `experiments/configs/monitor_ladder.json` | `analysis/aggregate.py`, `analysis/paired.py`, `analysis/plot_campaign.py` |
| Rebuild decomposition | Perfect V1/V2 vs static/oracle | `experiments/configs/perfect_detector_template.json`, blocked until measured dynamic-broad phase oracle | `analysis/phase_oracle.py`, `analysis/aggregate.py`; real rebuild time charged |
| Scale curve | 100K, 1M, 5M | `experiments/configs/scale.json` | `analysis/aggregate.py`, `analysis/plot_campaign.py` |
| Cloud replication | repeated selected configs on separate CPU group | selected config | normalized paired analysis only |

Legacy paper figures under `sigmod_submission/figures/` and `paper-final/figures/` come from earlier CSVs/scripts. Keep them separate from the new V2 campaign until revalidation.

The zero-cost envelope is a retrospective choice among measured whole-regional static layouts, not a deployable index or a claim of a universal upper bound. The executable perfect detector applies the selected fanout to all regions and charges actual rebuild time. Its oracle CSV must match machine, source, compiler flags, workload, seed and sample-rate stratum; one measured oracle repetition may supply choices to repeated runs of the same deterministic operation stream. `plot_maps.py` writes one figure per homogeneous stratum, with a short hash suffix in the filename.
