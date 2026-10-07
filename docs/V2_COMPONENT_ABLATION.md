# Adaptive-V2 Component Ablation

Status: **PREPARED, NOT EXECUTED**.

## Question

Which recurring components keep Adaptive V2 below Static when no rebuild occurs?
The study uses real prefixes of the production path; it does not add fake work.

| Variant | Real enabled path |
|---|---|
| `STATIC` | monolithic project B+ tree |
| `STATIC-REGIONAL` | eight project trees plus key-range routing |
| `V2-DATAPATH` | V2 segment objects and routing; monitor/controller/rebuild off |
| `V2-MONITOR` | V2 data path plus unchanged bounded sampled monitor |
| `V2-CONTROLLER` | monitor plus unchanged policy/gate; accepted rebuilds are counted but suppressed |
| `ADAPT-V2` | unchanged full V2 behavior |

`STATIC-REGIONAL` and `V2-DATAPATH` are retained because their concrete object
layout and call path differ. `V2-MONITOR` and `V2-CONTROLLER` are separable:
the former never invokes `consider`, while the latter records evaluations and
all gate outcomes. Default `ADAPT-V2` behavior is unchanged.

## Frozen matrix

- 1M records; 1M measured operations; Zipf 0.99; 95% reads/5% existing-value updates.
- Fanout 64; eight segments; sample rate 32; interval 5,000.
- Seeds: 11, 23, 37, 53, 71; five repetitions; six variants.
- Total: **150 runs** (25 exact six-way groups).

Five repetitions per seed are sufficient initially because the existing 1M
selected-regime coefficients of variation are generally near 1--2%. The 25
paired observations support a hierarchical bootstrap while treating seed, not
each repetition, as the workload-level unit. Ten repetitions are not justified
unless the new campaign's within-seed CV exceeds 5% under a predefined audit.

## Measurement and interpretation

Primary evidence is uninstrumented end-to-end throughput and sampled latency.
Existing V2 monitor timing remains a sparse software estimate and is reported
only as a diagnostic. Do not subtract independent row means as exact component
costs. Report paired adjacent contrasts with hierarchical 95% bootstrap CIs and
state that cache placement and compiler optimization make the ladder non-additive.

Expected zero-activity checks:

- `V2-DATAPATH`: monitor events, samples, evaluations, and rebuilds are zero.
- `V2-MONITOR`: evaluations and rebuilds are zero.
- `V2-CONTROLLER`: evaluations are positive; rebuild count is zero; any accepted
  actions appear in `rebuilds_suppressed`.
- all rows: zero misses and matching workload fingerprint/checksum within group.

## Manual execution

```bash
bash scripts/linux/build_icde_eab.sh unified
bash scripts/linux/run_v2_component_ablation.sh --execute v2_overhead
python3 analysis/v2_component_ablation.py \
  --raw results/raw/v2_overhead --out results/processed/v2_overhead
```

Checkpointing is one immutable CSV per run. Rerunning skips completed CSVs;
`.partial.csv` or `.failed.json` stops the campaign for manual inspection.
Archive the raw campaign directory and environment metadata before analysis.

Outputs: `v2_component_summary.csv`, `v2_component_diagnostics.csv`, and
`v2_component_ablation.pdf`.
