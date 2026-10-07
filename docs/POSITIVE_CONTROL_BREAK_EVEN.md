# Positive Control and Break-Even Boundary

Status: **CALIBRATION PREPARED; EVALUATION FAIL-CLOSED; NOT EXECUTED**.

## Hypothesis and bias control

A larger resident set, concentrated 1% moving hot range, and one-million-
operation phases may create more fanout opportunity than the completed studies.
This is plausible cache-working-set pressure, not a controller change. The V2
candidate set, monitor, sampling, hysteresis, cooldown, cost gate, and rebuild
path remain unchanged.

Calibration uses only seeds 101, 103, and 107. It compares 1M and 5M records at
fanouts 8--256. The predefined selector chooses the record count with the
largest mean best-static/f64 ratio and requires at least 1.10 mean headroom.
If neither passes, evaluation stops with outcome E; evaluation seeds are never
examined. This avoids seed and workload cherry-picking.

## Stage C1: calibration

- Two record counts x six fanouts x three seeds x one repetition.
- 10M operations; shifting; phase length 1M; Zipf 0.8; hot fraction 0.01.
- Total: **36 runs**.

```bash
bash scripts/linux/build_icde_eab.sh unified
bash scripts/linux/run_positive_control.sh calibration --execute
python3 analysis/select_positive_control.py \
  --raw results/raw/positive_control --root .
```

The selector either creates frozen `positive_control_oracle_grid.json` and
`positive_control_evaluation.json`, or records that no credible control exists.
Do not edit generated configs after seeing evaluation data.

## Stage C2: evaluation, only if selected

Oracle grid: six static regional fanouts x five evaluation seeds x five
repetitions = **150 runs**. Then exact `STATIC-REGIONAL`/`PERFECT-V2`/
`ADAPT-V2` execution uses 25 trios = **75 runs**, but the static member is
already available from the f64 oracle grid and must be reused, not rerun.
Therefore only Perfect-V2 and Adaptive-V2 add **50 runs**, for **200 evaluation
runs total**.

```bash
bash scripts/linux/run_positive_control.sh oracle --execute
python3 analysis/phase_oracle.py \
  --raw results/raw/positive_control \
  --family icde_positive_control_oracle_grid \
  --output results/processed/positive_control/oracle.csv
bash scripts/linux/run_positive_control.sh evaluation --execute
python3 analysis/positive_control.py \
  --raw results/raw/positive_control \
  --oracle results/processed/positive_control/oracle.csv \
  --out results/processed/positive_control
```

The evaluation config contains only Perfect-V2 and Adaptive-V2. The analysis
joins exact f64 rows from the oracle-grid root, preventing duplicate static
runs while preserving exact seed/repetition pairing.

## Analysis

For compatible elapsed-time measurements, compute operation opportunity
`Delta`, recurring overhead `mu`, and paid rebuilding `R/T`. Break-even requires
`Delta > mu` and `T > R/(Delta-mu)`. Report the theoretical condition, measured
estimate, and end-to-end ratios separately. `Perfect-V2` is a perfect detector
with paid actuation, not an optimal cost-aware policy.

Primary figure: `opportunity_vs_cost.pdf`, containing only measured points and
the equality boundary. Outcomes A--E in the strengthening plan are all valid.
