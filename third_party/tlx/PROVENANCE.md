# TLX dependency provenance

Status: **SOURCE NOT VENDORED; MANUAL RETRIEVAL REQUIRED**.

- Upstream: `https://github.com/tlx/tlx`
- Frozen version: tag `v0.6.1`
- License: Boost Software License 1.0
- Expected checkout: `third_party/tlx/src`
- Local modifications: none. The project adapter is
  `benchmarks/benchmark_external_tlx.cpp`.

Before building, retrieve and verify the dependency manually:

```bash
git clone --branch v0.6.1 --depth 1 https://github.com/tlx/tlx third_party/tlx/src
git -C third_party/tlx/src rev-parse HEAD > third_party/tlx/RESOLVED_COMMIT.txt
git -C third_party/tlx/src status --porcelain
```

The last command must print nothing. Record the resolved full SHA in the
campaign archive. The build script refuses to proceed if the checkout, license,
tag, or resolved-commit record is absent. Do not substitute a system package or
an unpinned branch.
