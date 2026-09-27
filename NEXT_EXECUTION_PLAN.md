# Smallest Remaining Execution Plan

## A. Codespaces correctness, Clang, and sanitizers

- Run the existing CI matrix unchanged in strength: GCC, Clang `-Werror`,
  ASan/UBSan/leak detection, semantic checksum smoke, and the prepared
  concurrent final-state/invariant test.
- Add one TSan job for the B+ tree concurrency test. Treat any race as a
  correctness defect, not as a warning to suppress.
- Preserve logs and the concurrent evidence CSV.

## B. Adaptive scale campaign

- On one controlled machine/binary, run STATIC-REGIONAL, ADAPT-V1, and ADAPT-V2
  at 100K, 1M, and 5M records using matched seeds/repetitions and one fixed
  selected regime.
- Do not recreate the completed broad static, dynamic-screening, or selected
  oracle/Perfect campaigns.

## C. Independent-platform replication

- Replicate only the three selected regimes with STATIC-REGIONAL and ADAPT-V2
  on one independent CPU platform, preserving compiler, source, machine,
  workload fingerprint, checksum, and miss metadata.

## D. PVLDB-only phase-length priority

- If targeting PVLDB after A--C, run one paired phase-length sweep around the
  break-even boundary for STATIC-REGIONAL, PERFECT-V2, and ADAPT-V2.
- This is the only next sensitivity campaign; all earlier 4,440 substantive
  Kaggle runs remain authoritative and must not be recreated.
