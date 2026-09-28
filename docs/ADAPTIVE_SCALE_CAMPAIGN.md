# Adaptive Scale Campaign

Status: **COMPLETE AND VERIFIED**.

## Verified completion record

- Raw evidence archives: `adaptive_scale_results.zip` and
  `adaptive_scale_resume.zip`.
- Git commit: `56dd79874a5162d0f1ffe186653e53beb90a7b73`.
- Kaggle machine fingerprint: `f410f7064c081e00`.
- Matrix: 3 scales x 5 seeds x 10 repetitions x 2 variants = 300 primary
  rows, comprising 150 exact `STATIC`/`ADAPT-V2` pairs.
- Sidecars: 300 phase files.
- Integrity: 0 partial results, 0 failure markers, and all 901 result payload
  files are byte-identical between the two archives by SHA-256.
- Correctness: 0 misses; checksum and workload fingerprint match within every
  exact pair.
- Analysis: 10,000-draw hierarchical paired bootstrap, resampling seeds and
  then repetitions within seed, with RNG seed 20270928.

| Records | Static mean | Adaptive V2 mean | V2/Static | Paired 95% CI | Mean rebuilds |
|---:|---:|---:|---:|---:|---:|
| 100K | 5.85M ops/s | 4.96M ops/s | 0.848 | 0.839--0.857 | 1.0 |
| 1M | 3.39M ops/s | 3.00M ops/s | 0.888 | 0.877--0.900 | 0.0 |
| 5M | 2.45M ops/s | 2.27M ops/s | 0.926 | 0.912--0.940 | 0.0 |

Adaptive V2 remains below parity at every tested scale, while its penalty
narrows from approximately 15.2% at 100K to 7.4% at 5M. The zero rebuild count
at 1M and 5M attributes the residual paired loss to recurring online work, not
to measured reconstruction. The current schema does not separately time
routing and controller work, so their individual shares are not inferred.

The historical six-fanout sweep indicates 4.3--5.8% mean static-oracle
headroom. It is `HISTORICAL_UNPAIRED` context only: source fingerprints and
repetition structures differ, so it is not an exact comparison with the scale
cohort.

## Scientific question and hypotheses

The primary question is whether the already-defined Adaptive V2 remains slower
than a strong fixed-fanout baseline as the database grows from 100K to 1M and
5M records. The secondary question is whether larger databases create enough
static fanout opportunity to make online adaptation economically plausible.

The hypotheses are frozen before execution:

1. Adaptive V2 remains below the fixed baseline at all tested scales.
2. Static fanout headroom may grow with scale, but online recurring and rebuild
   costs may still prevent V2 from capturing it.
3. A confidence interval crossing parity is inconclusive, not evidence of
   parity or a win.

## Audit and reused evidence

The previous `scale.json` was an unexecuted broad scaffold containing STATIC,
ADAPT-V1, and ADAPT-V2. It did not explicitly freeze the read/update mix,
phase semantics, segment count, adaptation interval, latency sampling, or
candidate set. It has been replaced by the matrix below.

The completed static-opportunity campaign contains 1,080 rows and sweeps
fanouts 8, 16, 32, 64, 128, and 256 at all three required scales. Its target
subset uses the same deterministic generator, 1M operations, Zipf 0.99, 95/5
read/update mix, five seeds, 10K warmup, and the same candidate set. Those rows
support a provenance-preserving **historical zero-cost static oracle**.

They are not exact performance pairs for the new campaign. The old grid has
only repetition 0 and was built from an older source fingerprint. Therefore,
new STATIC f=64 anchors are mandatory. The analysis labels the old oracle
`HISTORICAL_UNPAIRED` and never inserts it into a new exact pair.

Existing selected-regime V1/V2 and the verified 300-row Perfect decomposition
are not rerun. Perfect-V2 is excluded because scale does not change the already
established distinction between perfect detection and paid actuation; adding it
would multiply runs without answering the primary V2-versus-static question.
V1 is excluded because it has already served as the design ablation.

## Frozen matrix

| Dimension | Frozen value |
|---|---|
| Experiment family | `adaptive_scale` |
| Variants | `STATIC`, `ADAPT-V2` |
| Records | 100,000; 1,000,000; 5,000,000 |
| Measured operations | 1,000,000 |
| Workload | stationary Zipf, theta 0.99 |
| Reads / updates | 0.95 / 0.05 |
| Seeds | 11, 23, 37, 53, 71 |
| Repetitions | 10 per seed |
| Initial/static fanout | 64 |
| Candidate fanouts | 8, 16, 32, 64, 128, 256 |
| Segments | 8 |
| Sample rate | 1/32 |
| Adaptation interval | 5,000 segment accesses |
| Warmup | 10,000 operations |
| Latency sampling | 1/128 |
| Phase length | 0 (no shifting phases) |
| Hot fraction | 0.20 (generator parameter; irrelevant to stationary Zipf ranking) |
| Threads | 1 |

V2 is frozen exactly as implemented: eight segment-local bounded monitors, each
with Count-Min dimensions 256 by 4, heavy-key capacity 16, two-vote hysteresis,
four-interval cooldown, the existing cost gate, and the six candidate fanouts.
No setting may be changed after results are observed.

Ten repetitions are retained because the completed selected campaign used ten
per seed and its representative throughput coefficients of variation were
nonzero. Five seeds by ten repetitions permits paired hierarchical resampling
without inventing a new precision standard.

## Run count

Each scale has 5 seeds x 10 repetitions x 2 variants = **100 new runs**.

| Scale | STATIC | ADAPT-V2 | Total |
|---|---:|---:|---:|
| 100K | 50 | 50 | 100 |
| 1M | 50 | 50 | 100 |
| 5M | 50 | 50 | 100 |
| **Total** | **150** | **150** | **300** |

Perfect-V2: 0. Adaptive V1: 0. New six-fanout sweeps: 0.

A wall-clock estimate is unavailable from current repository evidence: prior
artifacts preserve result rows but do not establish a sufficiently stable
per-scale campaign duration for this exact source and Kaggle allocation.

## Identity, output, and restart contract

The unique experiment identity is the SHA-256 digest of:

`family + full parameter tuple + repetition + variant + machine fingerprint + source fingerprint`.

Timestamps are never identity fields. The runner writes each result first as
`<run-id>.partial.csv`, requires its phase sidecar, appends Git provenance, and
then atomically renames both files. Completed `<run-id>.csv` files are skipped.
A leftover partial stops the campaign and must be inspected; it is never
accepted as evidence. `environment.json` freezes machine, source, compiler
flags, ordering seed, Git commit, binary path, CPU description, and full config.
Changing any of them requires a new campaign directory.

Raw output is placed under:

`results/raw/kaggle/<machine-fingerprint>/adaptive_scale/`

Resume by restoring the previous `results/` tree into the same repository path
and rerunning the identical command. Do not merge outputs from different
machine fingerprints. The final notebook cell creates both
`adaptive_scale_results.zip` and `adaptive_scale_resume.zip`.

## Correctness gates

Analysis fails closed unless all 150 exact identities contain both variants.
Every pair must match machine, source, compiler/version/flags, workload
fingerprint, checksum, and zero misses. Scale, workload, seed, repetition,
fanout, candidate set, sampling, and segmentation must match the frozen matrix.
No invalid or incomplete pair is included.

The benchmark reports `NOT_CHECKED` for structural invariants during timing;
that is intentional and does not replace the separately validated correctness
suite. This performance campaign does not alter the validated concurrency
scope and is single-threaded.

## Reproduction procedure

Use accelerator **None**. Upload a source bundle containing this committed
repository and import `kaggle/kaggle_adaptive_scale.ipynb`. Do not run it in a
session containing unrelated active work.

The primary command used by the notebook is:

```bash
bash scripts/linux/run_scale.sh --execute kaggle
```

For a dry-run count before execution:

```bash
bash scripts/linux/run_scale.sh dry-run kaggle
```

If the source archive has no `.git` directory, set `CABTREE_GIT_COMMIT` to the
commit used to build the upload. The notebook reads `campaign_git_commit.txt`
when present and refuses to begin the campaign without explicit commit
provenance.

## Verified post-run analysis

After restoring the completed raw results and retaining the original completed
static archive, run manually:

```bash
python3 analysis/adaptive_scale.py \
  --new-raw adaptive_scale_results.zip \
  --static-archive results.zip \
  --out-dir results/processed/adaptive_scale_20260929
```

The script uses 10,000 hierarchical paired bootstrap draws with deterministic
RNG seed 20270928. It resamples seeds and then repetitions within seeds. It
emits:

- `results/processed/adaptive_scale_20260929/adaptive_scale_pairs.csv`
- `results/processed/adaptive_scale_20260929/adaptive_scale_per_seed.csv`
- `results/processed/adaptive_scale_20260929/adaptive_scale_oracle.csv`
- `results/processed/adaptive_scale_20260929/adaptive_scale_summary.csv`
- `results/processed/adaptive_scale_20260929/adaptive_scale_table.csv`
- `results/processed/adaptive_scale_20260929/adaptive_scale_verification.md`

The optional figure command is:

```bash
python3 analysis/plot_adaptive_scale.py \
  --summary results/processed/adaptive_scale_20260929/adaptive_scale_summary.csv \
  --output sigmod_submission/figures/adaptive_scale.pdf
```

The scale-sensitivity subsection is integrated after the existing
selected-regime/Perfect decomposition. The verified table is
`sigmod_submission/tables/adaptive_scale.tex`.

## Frozen decision rules

For each scale, let the paired statistic be `ADAPT-V2 throughput / STATIC
throughput`.

- CI entirely below 1: V2 remains slower at that tested scale.
- CI crosses 1: inconclusive; do not call parity or a win.
- CI entirely above 1: adaptation may pay, but first audit identity,
  correctness, baseline, rebuilds, oracle consistency, outliers, and machine.

Interpret scale trends without forcing the current narrative:

1. Tiny oracle headroom everywhere: opportunity remains limited.
2. Growing oracle headroom but V2 remains slower: opportunity exists but online
   costs still prevent capture.
3. V2 approaches parity: scale may improve amortization; narrow the claim.
4. V2 reliably exceeds static at 5M: report a scale-dependent boundary.

The analysis reports paired total latency penalty, measured rebuild cost per
operation, and a derived recurring penalty equal to total paired penalty minus
measured rebuild time. Direct routing and controller timing are unavailable in
the current schema and must not be fabricated. `monitor_work_ms` is preserved
as measured instrumentation.

## Unresolved risks

- The historical oracle and new V2 campaign cannot be exact pairs because the
  source fingerprints and repetition structures differ. It is contextual
  opportunity evidence only.
- One million operations at every scale reduces operations per resident key as
  scale grows. The experiment tests the frozen repository scale design, not a
  constant-operations-per-key design.
- Kaggle hardware can change across sessions. A changed machine fingerprint
  requires a distinct cohort; performance rows must not be pooled.
- The completed campaign demonstrates that the 5M preload and V2 segmentation
  fit this Kaggle allocation; it does not establish a memory bound for other
  machines.
- Git commit provenance requires the upload packager to preserve the commit in
  `campaign_git_commit.txt` when `.git` is absent.
