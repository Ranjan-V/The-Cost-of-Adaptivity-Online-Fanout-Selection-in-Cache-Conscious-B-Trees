# Test plan

All new tests: NOT_EXECUTED. `tests/test_v2.cpp` covers empty/one-key/boundary/multilevel bottom-up loads, sorted export, lookup and range equivalence, updates, deletion of one key, invalid sorted input, bounded monitor deterministic estimates/space/reset, and V1/V2 equivalence under forced rebuild.

Before a full campaign, add and run randomized model-based tests for insert/update/delete/range with an ordered reference map; check internal-node split invariants and leaf-chain order after bulk load and subsequent inserts. Validate workload fingerprint equality across variants and seed changes, parser rejection of malformed CLI/config, run-ID uniqueness, resume after interrupted run, checksum equality, phase-target transitions, and memory growth on 5M keys. Threaded tests require a separate lifetime/reclamation review; new V2 is single-threaded.
