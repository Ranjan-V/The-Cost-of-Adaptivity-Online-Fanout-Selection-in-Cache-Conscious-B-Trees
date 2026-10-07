# ICDE EAB Strengthening Plan

Status: **ALL THREE STUDIES PREPARED, NOT EXECUTED**.

The intended narrative is: **Opportunity, Observation, and Actuation: When Is
Online Index Adaptation Worth Its Cost?** Existing evidence is immutable and is
used as calibration/context only.

## Audit

### Reusable infrastructure

`benchmark_unified.cpp` already provides deterministic Zipf/shifting traces,
workload fingerprints, checksums, misses, latency sampling, V2 monitor/gate/
rebuild counters, memory estimates, and phase sidecars. `campaign.py` provides
randomized variant order, machine/source fingerprints, immutable per-run CSVs,
failure markers, and checkpoint/resume. Existing paired and phase-oracle scripts
provide the statistical model reused here.

### Reviewer objections and evidence gaps

| Objection | Already known | New study | Remaining boundary |
|---|---|---|---|
| Residual V2 cost is broadly attributed, not causal | zero rebuilds at 1M/5M; V1 ladder | six-level V2 path ablation | software timings remain perturbative and components are non-additive |
| Project static tree may be weak | multiple project fanouts and platforms | paired TLX B+ tree reference | cross-implementation layout differs; not an adaptive-index generalization |
| No observed profitability boundary | oracle/Perfect/Adaptive phase study is entirely below parity | preregistered high-opportunity calibration/evaluation | study stops honestly if no candidate reaches 10% calibration headroom |

The completed scale and phase-length campaigns already show that rebuild cost
is not the sole explanation. They do not isolate routing, monitoring, and gate
evaluation individually, provide an independent codebase, or cover both sides
of a break-even boundary.

## Frozen execution stages

### A. V2 component ladder

Six variants, five seeds, five repetitions: **150 runs**. See
`V2_COMPONENT_ABLATION.md`. This is the highest-information and highest-priority
study because it directly tests the manuscript's central causal explanation.

### B. TLX external reference

Two variants, five seeds, five repetitions: **50 runs**. It is blocked only on
manual retrieval and SHA recording of official TLX tag `v0.6.1`. See
`EXTERNAL_INDEX_BASELINE.md`.

### C. Positive control

Calibration is **36 runs**. If and only if the frozen 10% rule succeeds, the
oracle grid is 150 runs and paid Perfect/Adaptive evaluation is 50 runs, for
**200 evaluation runs**. See `POSITIVE_CONTROL_BREAK_EVEN.md`.

Total maximum new executions: **436 runs**. If calibration fails eligibility,
the total is **236 runs**, and outcome E is reported.

No exact runtime is claimed. Existing throughput does not determine TLX build
time or the new 5M/10M calibration cost sufficiently well, and inventing a
wall-clock estimate would be misleading. Each stage is independently
checkpointed and can be stopped between stages.

## Provenance and exclusion policy

All comparisons require matching machine, source, flags, workload definition,
seed, repetition, fingerprint, checksum, and zero misses. Exclude only failed,
partial, fingerprint-mismatched, checksum-mismatched, or nonzero-miss rows;
retain their logs and never exclude for performance. Bootstrap RNG seeds are
fixed. Seeds are resampled hierarchically, then repetitions within seed. Raw
throughput is never pooled across machines.

## Manual run order

```bash
bash scripts/linux/build_icde_eab.sh unified
bash scripts/linux/run_v2_component_ablation.sh --execute v2_overhead
# Retrieve TLX exactly as documented, rebuild, then:
bash scripts/linux/run_external_tlx.sh --execute
bash scripts/linux/run_positive_control.sh calibration --execute
# Run selector; continue with oracle/evaluation only if it freezes configs.
```

Full analysis commands are in each study document. Resume uses the same command:
completed CSVs are skipped; any partial/failure marker blocks continuation.

## Paper integration plan

Do not modify `sigmod_submission/main.tex` until results exist.

1. Add the V2 ladder after the current clean ablation and replace broad causal
   attribution only if adjacent paired CIs support it. Figure:
   `results/processed/v2_overhead/v2_component_ablation.pdf`.
2. Add TLX as an external-reference paragraph/table in methodology/results;
   explicitly preserve the distinction between external static credibility and
   within-prototype causality. Figure: `external_tlx.pdf`.
3. Extend the break-even section with the preregistered outcome A--E and the
   opportunity-versus-cost plot. Do not claim a boundary crossing unless paid
   end-to-end evidence crosses parity.

Scaffolds live under `paper_scaffolds/` and are not included by the manuscript.

## Risks

- Timer instrumentation perturbs the path; end-to-end rows remain primary.
- TLX's native node policy is not fanout-matched and may be faster or slower for
  reasons other than implementation quality.
- Calibration may find no eligible high-headroom workload; this is a valid stop.
- A 5M-record, 10M-operation candidate may be memory-sensitive on small hosts.
- Perfect-V2 is detector-perfect, not an optimal cost-aware controller.
