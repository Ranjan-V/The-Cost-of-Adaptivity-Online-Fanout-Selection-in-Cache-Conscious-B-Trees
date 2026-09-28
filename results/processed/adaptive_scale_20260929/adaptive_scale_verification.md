# Adaptive-scale verification

- New exact pairs: 150 (50 per scale).
- New machines: `f410f7064c081e00`.
- New source IDs: `fcc52a7d6e6685418f60`.
- Correctness: zero misses and matching checksums/fingerprints within every pair.
- Bootstrap: hierarchical paired bootstrap, 10000 draws, RNG seed 20270928.
- Oracle: six-fanout static opportunity sweep, retained as `HISTORICAL_UNPAIRED`; it is not merged into exact new pairs because source/repetition identity differs.
- Direct routing/controller timing is unavailable in the current schema. `derived_recurring_penalty_us_op` is the paired total latency penalty minus measured rebuild time per operation.
