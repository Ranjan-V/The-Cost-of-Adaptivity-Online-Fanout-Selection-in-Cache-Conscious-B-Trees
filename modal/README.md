# Modal overflow screening

This package fans out independent, CPU-only static and static-regional runs.
It is for broad opportunity discovery, not final latency or hardware-counter
claims. Absolute throughput must remain grouped by the CPU fingerprint recorded
in each CSV.

## Safety properties

- Dry-run is the default. Remote work begins only with `--execute`.
- The app requests one CPU and no GPU per function.
- `max_containers=32` bounds concurrency and spend.
- Every job has a deterministic ID and distinct CSV, phase sidecar, and log.
- Completed output is never overwritten; a partial output stops that job.
- Results persist in a named Modal Volume v2.
- The initial grid contains only non-adaptive variants. It discovers fanout
  opportunity without spending compute on policies that cannot create it.

## Local preparation (does not start cloud compute)

```powershell
cd D:\Research\SIGMOD
.\venv\Scripts\Activate.ps1
python modal\plan.py --config modal\configs\overflow_grid.json `
  --manifest results\manifests\modal_overflow_plan.json
```

Inspect the job count and manifest before authenticating or executing anything.

## Modal setup

Install and authenticate the current Modal client manually:

```powershell
python -m pip install modal
python -m modal setup
python -m modal volume create cabtree-overflow-results-v2 --version=2
```

Preview through Modal without invoking remote functions:

```powershell
python -m modal run modal\modal_campaign.py `
  --config modal/configs/overflow_grid.json
```

Run a two-job paid smoke test only after code review:

```powershell
python -m modal run modal\modal_campaign.py `
  --config modal/configs/overflow_grid.json --max-jobs 2 --execute
```

The full grid must not be launched until the two-job result has been downloaded,
checked for zero misses and matching fingerprints, and its likely cost has been
reviewed. To download persisted results:

```powershell
python -m modal volume get cabtree-overflow-results-v2 raw\modal-overflow-v1 `
  results\raw\modal\modal-overflow-v1
```

Current API choices follow Modal's official documentation for CPU functions,
`Image.add_local_dir(..., copy=True)`, mapped function invocation, and Volume
v2 concurrent writes. No deployment or cloud execution is performed while
preparing this package.
