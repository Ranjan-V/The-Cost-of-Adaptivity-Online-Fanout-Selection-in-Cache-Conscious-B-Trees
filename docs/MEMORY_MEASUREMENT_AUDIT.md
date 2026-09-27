# Existing Memory Measurement Audit

This audit covers existing rows in `results-now-2.zip`; it does not infer
unrecorded allocator state. Aggregate values are in
`results/processed/perfect_memory_summary.csv`, and the manuscript-ready table
is `sigmod_submission/tables/perfect_memory.tex`.

| Quantity | Existing measured evidence | Interpretation |
|---|---|---|
| Peak RSS / working set | Supported for all matched STATIC-REGIONAL, ADAPT-V1/V2, and PERFECT-V1/V2 rows | Linux `ru_maxrss`, converted to bytes; process-level peak, not component attribution |
| Tree bytes | Supported for STATIC-REGIONAL, ADAPT-V2, and PERFECT-V2 | Internal ownership estimate from node/value counts; V1 is `UNSUPPORTED` |
| Bytes/key | Supported for STATIC-REGIONAL, ADAPT-V2, and PERFECT-V2 | Sum of supported tree, monitor, and shadow estimates divided by records; not RSS/key |
| V1 shadow-record state | `8,000,000` bytes at one million records | Lower-bound `pair<const int,int>` payload only; excludes hash buckets, nodes, and allocator overhead |
| V1 monitor state | `UNSUPPORTED` | No empirical component measurement exists |
| V2 bounded monitor total | `68,416` bytes across eight segments | Existing aggregate includes monitor object, sketch capacity, and reserved heavy-hitter capacity |
| V2 sketch alone | Not separately measured | Source dimensions are known, but splitting the aggregate would be a model-derived value, not a measurement |
| V2 heavy hitters alone | Not separately measured | Same limitation as sketch |
| Rebuild temporary buffer | Supported for V2 as exported-vector capacity | Excludes simultaneously allocated replacement-tree nodes and allocator overhead; therefore a lower bound |
| V1 rebuild temporary buffer | `UNSUPPORTED` in substance | A recorded zero does not measure V1 replay/hash-map temporary state |

Representative matched means show STATIC-REGIONAL at about 72.3--72.4 MiB
peak RSS, ADAPT-V2 at 72.3--72.4 MiB, ADAPT-V1 at 116.1--122.7 MiB,
PERFECT-V2 at 78.4--78.7 MiB, and PERFECT-V1 at 132.7--140.9 MiB across
the three regimes. These are process peaks and must not be presented as exact
component sums. In particular, lower V2 tree bytes after Perfect runs reflect
the final fanout/layout, while RSS includes transient rebuild allocation.
