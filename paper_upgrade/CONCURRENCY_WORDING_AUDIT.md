# Concurrency Wording Audit

| Wording/claim | Classification | Required treatment |
|---|---|---|
| Historical concurrent benchmark throughput | NEEDS_VALIDATION | It used the earlier optimistic value-read path; rerun the revised leaf-locked path before publication |
| Preloaded lookup/existing-value-update mode | NEEDS_VALIDATION | Static synchronization argument is sound, but revised TSan evidence is pending |
| Generic "thread-safe B+ tree" | UNSUPPORTED | Only the explicitly scoped preloaded mode is intended to be concurrent |
| "lock-free reads" / "latch-free readers" | UNSUPPORTED | Supported readers acquire the target leaf lock |
| "production-grade" / "production-style" | SHOULD_BE_SOFTENED | The research prototype excludes concurrent structural change and reclamation |
| Linearizable operations | UNSUPPORTED | The prepared test validates deterministic post-join state, not histories |
| Concurrent adaptation | UNSUPPORTED | Adaptive replacement remains quiescent and single-threaded |

Concurrent structural insertion, splitting, removal, and adaptive replacement
are unsupported. Do not convert a clean scoped TSan run or final-state test
into a general thread-safety or linearizability claim.
