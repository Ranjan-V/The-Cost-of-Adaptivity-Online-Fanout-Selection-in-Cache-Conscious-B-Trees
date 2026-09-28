# ThreadSanitizer Runbook

TSan is a correctness tool, never a throughput tool. Expect substantial
slowdown and memory overhead.

Dry-run first:

```bash
bash scripts/linux/run_codespaces_validation.sh --dry-run tsan
```

Then run only the supported preloaded lookup/existing-value-update target:

```bash
bash scripts/linux/run_codespaces_validation.sh tsan
```

The script stores the TSan log and CSV under a new timestamped
`results/validation/` directory. Do not compare its elapsed time with any
benchmark. A race report, deadlock, crash, missing CSV, nonzero exit, or failed
reference/invariant field is a STOP condition. Do not suppress a report merely
to obtain green CI. If the Codespaces Clang runtime cannot support TSan, retain
the exact error log and classify the gate `UNSUPPORTED`, never `PASS`.

The earlier structural-insert diagnostic produced a genuine race between
`insert_in_node` and `find_leaf_optimistic`. Structural mutation is therefore
unsupported concurrently and is deliberately absent from the supported TSan
gate; this is scope restriction, not suppression of the finding.
