# Concurrency Wording Audit

| Wording/claim | Classification | Required treatment |
|---|---|---|
| Concurrent benchmark throughput and scaling numbers | SUPPORTED | Existing descriptive performance evidence, provided platform and run scope remain explicit |
| Version-counter optimistic read/retry mechanism exists | SUPPORTED | Describe implementation mechanics, not a formal guarantee |
| "thread-safe" B+ tree | NEEDS_VALIDATION | Requires clean Clang, sanitizer, concurrent final-state, and TSan gates |
| "lock-free reads" | SHOULD_BE_SOFTENED | Readers retry without taking the writer bit, but C++ data-race freedom and lock-free progress are not established |
| "latch-free readers" | NEEDS_VALIDATION | Mechanically plausible, but avoid until TSan and progress behavior are validated |
| "production-grade" / "production-style" | SHOULD_BE_SOFTENED | Current prototype lacks concurrent reclamation, concurrent adaptive replacement, and linearizability evidence |
| Linearizable operations | SHOULD_BE_SOFTENED | No history-based checker or proof exists; the prepared test validates deterministic post-join state only |
| Concurrent adaptation | SHOULD_BE_SOFTENED | Adaptive segment replacement remains single-threaded and requires epoch/hazard reclamation for readers |

The current manuscript already limits the thread benchmark to preloaded
lookups and in-place updates and states that concurrent adaptive replacement is
not evaluated. Preserve those limitations. Do not convert a clean controlled
test into a general linearizability claim.
