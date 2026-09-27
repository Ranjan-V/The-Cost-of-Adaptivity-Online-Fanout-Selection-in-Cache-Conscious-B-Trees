# ADAPT-V1 and ADAPT-V2

VALIDATION_STATUS: PARTIAL. Both implementations pass the local structural and paired semantic smoke tests. The selected-regime Kaggle validation is complete; broader cross-platform and controlled-laptop evaluation remains pending.

| Property | ADAPT-V1 | ADAPT-V2 |
|---|---|---|
| Source of records | B+ tree plus duplicate unordered map | B+ tree only |
| Rebuild input | Iterate duplicate map | Export sorted leaf records on approved rebuild |
| Rebuild construction | Ordinary insert replay | Bottom-up bulk load |
| Monitor | Legacy sampled map-based heat monitor | Fixed-size Count-Min sketch plus bounded candidate summary |
| Policy | Heat score, predictor, hysteresis/cooldown/heuristic cost gate | Dominant sample concentration, hysteresis/cooldown/heuristic cost gate |
| Concurrency | Segmented wrapper not synchronized | Segmented wrapper not synchronized |
| Experimental status | Legacy and selected-regime measured rows exist | Local correctness and selected-regime Kaggle campaign complete |

V2 deliberately changes the observation signal and controller while retaining the same key-range partition and candidate fanout range. A V1/V2 comparison therefore evaluates a package of engineering changes, not one isolated mechanism. Use the monitor ladder and real-rebuild perfect detector to attribute components. The V1 source is preserved apart from a diagnostic `force_rebuild()` entry point and rebuild-time accounting; its original map/replay policy is not overwritten.

V2's gate retains V1's heuristic coefficient but uses a linear record-count cost score because its rebuild path is bottom-up. Neither score is a calibrated time prediction. Later measurements must report decision errors and sweep gate sensitivity; do not treat the score inequality as a proof of profitable adaptation.
