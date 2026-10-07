# Adaptive-Scale Independent Replication

Status: **COMPLETE AND VERIFIED**.

## Verified result

- Raw primary rows: 90; phase sidecars: 90.
- Exact `STATIC`/`ADAPT-V2` pairs: 45 (15 per scale).
- Partial results: 0; failure markers: 0; misses: 0.
- Machine fingerprint: `8c1eaa02bef0eff8`.
- Source fingerprint: `e15d24b65699b5cf4d1d`.
- Git commit: `76a08ed4743fe90bf762e26ab8415993c5760502`.
- Evidence archive: `adaptive_scale_replication_results.zip`.
- Archive SHA-256:
  `984F7D40C2B7A6AC99213E8367A122BDF6AE6E5864B65C6C24B3F280B66C814F`.

| Records | Static mean | Adaptive V2 mean | V2/Static | Paired 95% CI | Result |
|---:|---:|---:|---:|---:|---|
| 100K | 2.82M ops/s | 2.65M ops/s | 0.949 | 0.909--1.033 | Inconclusive |
| 1M | 2.15M ops/s | 1.98M ops/s | 0.925 | 0.896--0.967 | Slower |
| 5M | 1.63M ops/s | 1.59M ops/s | 0.990 | 0.932--1.119 | Inconclusive |

All mean ratios remain below one and move generally toward parity with scale,
but only the 1M interval excludes parity. The independent platform therefore
supports the direction of the Kaggle result at 1M and does not contradict it
at 100K or 5M; it does not independently establish a significant loss at
those two scales.

This campaign repeats the verified Kaggle scale comparison on the local ASUS
i5-12500H/Windows platform. It is a separate cohort and must not be pooled with
Kaggle absolute throughput.

## Frozen matrix

- Variants: `STATIC`, `ADAPT-V2`
- Records: 100K, 1M, 5M
- Operations: 1M per run
- Seeds: 11, 23, 37, 53, 71
- Repetitions: 3
- Read/update mix: 95/5
- Zipf theta: 0.99
- Fanout: 64; candidates: 8, 16, 32, 64, 128, 256
- Segments: 8; sample rate: 1/32; adaptation interval: 5,000
- Total: 90 runs and 45 exact pairs

## Windows commands

Run from PowerShell in `D:\Research\SIGMOD`:

```powershell
Set-Location D:\Research\SIGMOD
.\.venv\Scripts\Activate.ps1

# Build the frozen C++11 release binary.
.\scripts\windows\build_release.ps1 -Execute

# Preview only. This must end with: planned runs: 90
.\scripts\windows\run_scale_replication.ps1 -Python .\.venv\Scripts\python.exe

# Execute. The runner checkpoints every row and skips completed rows on resume.
.\scripts\windows\run_scale_replication.ps1 `
    -Python .\.venv\Scripts\python.exe `
    -Execute

# Validate 45 exact pairs, generate the figure, and package evidence.
.\scripts\windows\finalize_scale_replication.ps1 `
    -Python .\.venv\Scripts\python.exe
```

Do not run other CPU-heavy work simultaneously. Keep the laptop plugged in,
use the same Windows power mode throughout, and close browsers and background
updaters. If interrupted, rerun the identical execute command; completed rows
are skipped. A `.partial.csv` or `.failed.json` requires inspection rather
than deletion.

Expected final artifacts:

- `results/raw/local/<machine>/adaptive_scale_replication/`
- `results/processed/adaptive_scale_replication/`
- `adaptive_scale_replication_results.zip`

## Runtime expectation

The completed Kaggle campaign produced 300 rows in approximately 9.4 minutes
of sequential wall time. This replication has 90 rows, but Windows process
startup, 5M-key preloading, laptop power limits, and background activity can be
slower. A reasonable planning estimate is **8--20 minutes** after compilation;
reserve **30 minutes** and stop if the machine begins swapping or thermal
throttling becomes severe. This is an estimate, not measured laptop evidence.

## Interpretation rule

Analyze paired ratios independently on the laptop. Reproduction means the
direction and scale trend are consistent; it does not require matching Kaggle
absolute throughput. If an interval crosses 1.0, report that scale as
inconclusive rather than forcing the existing narrative.
