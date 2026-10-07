# Independent Ordered-Index Baseline

Status: **PREPARED; BLOCKED ON MANUAL TLX RETRIEVAL; NOT EXECUTED**.

## Selection audit

- **TLX B+ tree selected.** It is the maintained successor to STX, provides a
  map-style ordered B+ tree, supports find/insert/existing-value assignment,
  has no required external runtime dependency, and is compatible with the
  project's C++11 Linux build.
- STX is obsolete and points users to TLX; using it would weaken maintenance
  provenance.
- Abseil is independently maintained and production-used, but current Abseil
  is C++17 and would violate this artifact's C++11 comparison cohort.
- ART/ALEX are different index families and are not substitutes for this exact
  static B+ tree credibility check.

Upstream is `https://github.com/tlx/tlx`, frozen tag `v0.6.1`, Boost Software
License 1.0. No TLX source is currently vendored. Follow
`third_party/tlx/PROVENANCE.md`; record the resolved full commit SHA and require
a clean checkout. The adapter is `benchmarks/benchmark_external_tlx.cpp` and
does not modify TLX.

## Fairness contract and frozen matrix

Both implementations use `int` keys/values, the identical generated operation
trace, preload keys 0..N-1, perform 95% lookup and 5% existing-value assignment,
and share timing boundaries, warmup, compiler, flags, host, seeds, and execution
order randomization. Construction time is reported separately. Checksums,
fingerprints, and misses must agree. The comparison is single-threaded because
cross-library concurrent semantics are not equivalent.

- 1M records; 1M operations; Zipf 0.99; fanout 64 for the project baseline.
- Seeds 11, 23, 37, 53, 71; five repetitions; two variants.
- Total: **50 runs** (25 exact pairs).

TLX chooses its own compile-time node layout; this is intentional for an
external optimized reference but prevents attributing differences solely to
fanout. This study tests whether the project static baseline is obviously weak;
it does not generalize the within-prototype adaptive-overhead claim to TLX.

## Manual execution

```bash
# First perform the manual retrieval in third_party/tlx/PROVENANCE.md.
bash scripts/linux/build_icde_eab.sh external
bash scripts/linux/run_external_tlx.sh --execute
python3 analysis/external_tlx.py \
  --raw results/raw/external_baseline --out results/processed/external_baseline
```

The runner resumes by skipping immutable completed CSVs and stops on partial or
failed rows. Preserve the TLX resolved SHA, license, environment JSON, raw CSVs,
and logs in the archive. Outputs are `external_tlx_summary.csv` and
`external_tlx.pdf`.
