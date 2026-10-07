# Phase-Length / Break-Even Campaign

Status: **COMPLETE AND VERIFIED**.

The CPU-only Kaggle campaign completed on 2026-10-07.  The evidence archives
are `phase_length_v2_results (1).zip` and `phase_length_v2_resume.zip`; their
SHA-256 digests are respectively
`f2efcdb23ccd2bceeeea424ebba45f8556d697e13ba56f5d0e1e01c60ff02623` and
`152a73792df2cf0cefb92b56c38b2c7e987e70576096f206aafbef936865f364`.
All 1,581 result files shared by the archives are byte-identical.

The run used AMD EPYC 7B12 machine fingerprint `f0521629f96fce19`, GCC 13.3,
C++11, and `-O3 -march=native -DNDEBUG`.  The packaged base commit was
`76a08ed4743fe90bf762e26ab8415993c5760502`; immutable row provenance records
source fingerprint `fcc52a7d6e6685418f60`.

Commit the prepared campaign files before execution and regenerate
`kaggle_upload_phase_length_v2.zip`. If an emergency uncommitted package is
used, its manifest explicitly records that it differs from the base commit;
the archived package and source fingerprint then become mandatory provenance.

## Question

At what workload phase length, if any, does fanout opportunity repay V2's
measured actuation and recurring online costs? The experiment separates:

1. `ORACLE-ZERO-COST / STATIC-REGIONAL`: available layout opportunity;
2. `PERFECT-V2 / STATIC-REGIONAL`: opportunity after real actuation cost;
3. `ADAPT-V2 / STATIC-REGIONAL`: opportunity after online control and actuation.

## Frozen matrix

Common settings: 1M records, 10M measured operations, shifting workload,
95/5 reads/updates, Zipf 0.99, hot fraction 0.05, sample rate 1/32, eight
segments, 10K warmup, fanout 64, and candidates 8--256.

Phase lengths are 10K, 100K, 1M, 5M, and 10M operations. Seeds are 11, 23,
37, 53, and 71.

### Stage A: measured regional oracle grid

- Six `STATIC-REGIONAL` fanouts
- One repetition
- 5 phase lengths x 5 seeds x 6 fanouts = **150 runs**

`analysis/phase_oracle.py` derives exactly 25 workload-paired zero-cost oracle
schedules from phase sidecars. The result is not an executable implementation.

### Stage B: paid decomposition

- `STATIC-REGIONAL`, `PERFECT-V2`, `ADAPT-V2`
- Five repetitions
- 5 phase lengths x 5 seeds x 5 repetitions x 3 variants = **375 runs**

Total: **525 runs**. V1 is excluded because it has already served as a design
ablation and does not answer the V2 break-even question.

## Execution order

Run on one CPU-only Kaggle session or resumed sessions with an identical
machine/source fingerprint. Do not pool changed machine fingerprints.

```bash
# Build, then dry-run Stage A. It must report 150.
bash scripts/linux/build_release.sh --execute
bash scripts/linux/run_phase_length_v2_oracle.sh dry-run kaggle

# Execute Stage A.
bash scripts/linux/run_phase_length_v2_oracle.sh --execute kaggle

# Derive the 25 measured oracle schedules.
python3 analysis/phase_oracle.py \
  --raw results/raw/kaggle \
  --family phase_length_v2_oracle_grid \
  --output results/processed/phase_length_v2_oracle.csv

# Dry-run Stage B. It must report 375, then execute it.
bash scripts/linux/run_phase_length_v2.sh dry-run kaggle
bash scripts/linux/run_phase_length_v2.sh --execute kaggle
```

The Stage B runner refuses to start Perfect-V2 if the measured oracle CSV does
not match machine, source, compiler flags, seed, workload, and phase count.

## Fail-closed requirements

- 150 Stage A and 375 Stage B primary rows;
- matching phase sidecars for every primary row;
- no `.partial.csv` or `.failed.json` files;
- zero misses;
- matching checksum and workload fingerprint inside each Stage B trio;
- exactly six fanouts per Stage A seed/phase-length cohort;
- exactly one 25-row oracle file;
- paired hierarchical bootstrap across seeds and repetitions;
- oracle rows treated as five seed-level measurements, not 25 replicated
  observations per phase length.

## Verified result

The archives contain 150 oracle-grid rows and 375 paid-decomposition rows,
with 525 matching phase sidecars and logs, zero partial/failure markers, and
zero suspicious failure signatures.  The paid rows form 125 exact
Static-Regional/Perfect-V2/Adaptive-V2 trios.  Every trio agrees on checksum
and workload fingerprint and reports zero misses.

| Phase length | Oracle/Static | Perfect-V2/Static | Adaptive-V2/Static | Perfect rebuilds |
|---:|---:|---:|---:|---:|
| 10K | 1.086 [1.060, 1.113] | 0.122 [0.075, 0.205] | 0.818 [0.792, 0.842] | 2344.0 |
| 100K | 1.038 [1.020, 1.055] | 0.259 [0.240, 0.277] | 0.790 [0.779, 0.802] | 473.6 |
| 1M | 1.078 [1.060, 1.096] | 0.593 [0.553, 0.641] | 0.792 [0.783, 0.802] | 54.4 |
| 5M | 1.055 [1.034, 1.074] | 0.728 [0.716, 0.744] | 0.778 [0.764, 0.796] | 9.6 |
| 10M | 1.087 [1.060, 1.115] | 0.769 [0.755, 0.785] | 0.781 [0.765, 0.797] | 8.0 |

Values are means with 95% hierarchical paired-bootstrap confidence intervals.
Adaptive V2 performed zero rebuilds in every cohort.  Perfect V2 approaches
Adaptive V2 as phases lengthen, but neither reaches static parity through the
tested 10M-operation phase.  This is an observed-range result, not a claim
about longer phases, other workloads, or other machines.

Processed evidence is in `results/processed/phase_length_v2_20261007/`; the
figure is `results/figures/phase_length_v2.pdf` and its manuscript copy is
`sigmod_submission/figures/phase_length_v2.pdf`.

## Interpretation rule

An interval below one is a loss; an interval crossing one is inconclusive.
Perfect-V2 crossing parity identifies a paid-actuation break-even point.
Adaptive-V2 crossing parity identifies an online break-even point. Do not
extrapolate beyond the five tested phase lengths or this machine/workload.

The execution commands above are retained for reproducibility.  Do not rerun
this completed campaign unless a separately documented replication requires
it.
