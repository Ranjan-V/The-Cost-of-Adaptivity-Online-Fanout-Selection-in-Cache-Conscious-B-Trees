# Cache-Adaptive B-Tree Results Summary

Source: YCSB trial summary, shifting trial summary, overhead trial summary, latest Wikipedia trace CSV, latest thread scalability CSV, latest sliding-hotspot CSV

## Main Takeaways

- Shifting workload: Segmented Adaptive is 0.92x Static B+Tree f=64.
- Shifting workload: Segmented Adaptive reaches 0.85x of the best oracle-hot row.
- Stationary YCSB: static fanout remains best or competitive in 3 of 3 workloads.
- Overhead ablation: monitor/rebuild costs remain a first-order limitation.
- Wikipedia trace: Segmented Adaptive is 0.81x Static B+Tree f=64 on recent pageview data.
- Thread scalability: optimistic lock coupling reaches 31.70M at 32 worker threads.
- Sliding hotspot: Segmented Adaptive is 0.63x Static B+Tree f=64.

## YCSB Stationary Workloads

| Workload | Static f64 | Segmented Adaptive | Seg. vs f64 | Best Index | Best vs f64 |
|---|---:|---:|---:|---|---:|
| YCSB-A | 4.70M | 3.83M | 0.81x | Static B+Tree f=64 | 1.00x |
| YCSB-B | 6.30M | 4.65M | 0.74x | Static B+Tree f=256 | 1.06x |
| YCSB-C | 7.04M | 4.74M | 0.67x | Static B+Tree f=64 | 1.00x |

## Shifting Workload Aggregate

| Index | Throughput | vs Static f64 | p95 us | Adapt | Rebuild |
|---|---:|---:|---:|---:|---:|
| Static Region f=256 | 5.53M | 1.13x | 0.295 | 0.0 | 0.0 |
| Static Region f=128 | 5.36M | 1.10x | 0.310 | 0.0 | 0.0 |
| Static Region f=32 | 5.31M | 1.08x | 0.315 | 0.0 | 0.0 |
| Oracle Hot f=32 | 5.29M | 1.08x | 0.355 | 5.0 | 5.0 |
| Static B+Tree f=256 | 5.24M | 1.07x | 0.315 | 0.0 | 0.0 |
| Oracle Hot f=16 | 5.08M | 1.04x | 0.345 | 5.0 | 5.0 |
| Oracle Hot f=8 | 5.01M | 1.02x | 0.365 | 5.0 | 5.0 |
| Static B+Tree f=64 | 4.89M | 1.00x | 0.345 | 0.0 | 0.0 |

## Overhead Ablation

| Workload | Region / Static | Region+Monitor / Region | SegNoRecords / Region | Records / NoRecords | Monitor+Records / Records | Full / Monitor+Records | Full Mean | 95% CI | CV% | Cost Skips | Rebuilds |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Uniform-B | 1.18x | 0.83x | 0.95x | 0.86x | 0.97x | 0.98x | 4.79M | 0.41M | 6.9 | 8.0 | 0.0 |
| Zipf-A | 1.01x | 0.87x | 0.97x | 0.91x | 0.94x | 0.85x | 3.91M | 0.30M | 6.2 | 1.0 | 1.0 |
| Zipf-B | 1.01x | 0.85x | 0.97x | 1.07x | 0.81x | 0.88x | 5.68M | 0.42M | 6.0 | 1.0 | 1.0 |

## Wikipedia Pageview Trace

| Index | Throughput | vs Static f64 | p95 us | Adapt | Rebuild |
|---|---:|---:|---:|---:|---:|
| Static B+Tree f=256 | 10.42M | 1.00x | 0.100 | 0 | 0 |
| Static B+Tree f=64 | 10.39M | 1.00x | 0.100 | 0 | 0 |
| Static Region f=32 | 10.19M | 0.98x | 0.100 | 0 | 0 |
| Static Region f=128 | 10.18M | 0.98x | 0.100 | 0 | 0 |
| Static Region x8 | 9.88M | 0.95x | 0.100 | 0 | 0 |
| Segmented No Adapt | 8.79M | 0.85x | 0.130 | 0 | 0 |
| Segmented Adaptive | 8.40M | 0.81x | 0.110 | 37 | 1 |

## Optimistic Lock Coupling Scalability

| Threads | Throughput | Speedup | Reads | Updates |
|---:|---:|---:|---:|---:|
| 1 | 3.14M | 1.00x | 9500349 | 499651 |
| 4 | 9.63M | 3.07x | 9500349 | 499651 |
| 8 | 17.48M | 5.57x | 9500349 | 499651 |
| 16 | 30.21M | 9.64x | 9500349 | 499651 |
| 32 | 31.70M | 10.11x | 9500349 | 499651 |

## Sliding Hotspot Benchmark

| Index | Throughput | vs Static f64 | p95 us | Adapt | Rebuild | Cost skips |
|---|---:|---:|---:|---:|---:|---:|
| Static B+Tree f=64 | 4.18M | 1.00x | 0.580 | 0 | 0 | 0 |
| Static Region x8 | 3.68M | 0.88x | 0.620 | 0 | 0 | 0 |
| Segmented Adaptive | 2.62M | 0.63x | 0.740 | 264 | 0 | 247 |
