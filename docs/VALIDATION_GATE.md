# Correctness and Portability Validation Gate

All evidence must come from the same checked-out commit and be retained under
timestamped `results/validation/` directories.

## Mandatory gates

- **GCC PASS:** configure, strict build, and all registered tests succeed.
- **Clang PASS:** configure, strict `-Werror` build, and all tests succeed.
- **ASan/UBSan PASS:** all tests exit zero with zero ASan errors, zero UBSan
  errors, and zero unexplained leaks.
- **Semantic identity PASS:** the deterministic STATIC/V1/V2 replay reports
  zero misses and matching checksums, sizes, and final range contents.
- **Concurrent correctness PASS:** every scenario reports expected count,
  matching checksum, zero expected misses, structural invariants, reference
  match, lookup check, and range check; no crash, deadlock, or timeout.
- **TSan PASS:** no data-race report in the controlled concurrent scenarios.
  If the runtime is unavailable, record `UNSUPPORTED` with its log. Unsupported
  is not PASS and blocks strong thread-safety claims.

## Stop conditions

Stop before scientific benchmarks on any GCC/Clang failure, sanitizer error,
unexplained leak, TSan race, checksum or miss mismatch, invariant or reference
failure, deadlock, crash, bulk-loader discrepancy, or V1/V2 semantic mismatch.
Do not weaken flags, skip tests, or suppress findings to cross the gate.
