# ICDE 2027 AWS Archive Coverage

Audited repository commit: `259bb6ddccda6d2fd48ea1d336db412839eb6e0f`.

## Original archives

| Cohort | Primary rows | SHA-256 | Extracted members compared |
|---|---:|---|---:|
| positive_control | 36 | `fe68d5cb7f7f0908cff12d538b6cf5b552cac9e18a21304bdbf755b2cda02b94` | 73 |
| v2_ablation | 150 | `8923397a1720b327e476d9277271ef6fc574829ea30ff7ba04f0692ec203db0b` | 301 |
| external_tlx | 50 | `73de07e98059c452f68d7f62fdbdae731a17f55c179f2fecfac7d48444c108d8` | 101 |
| logging_pilot | 9 | `d3d3dbc876311f5ae73d99a0a128b0a089f73053ff06e13d855b8dfa5b0ed4cf` | 0 |
| long_confirmation | 100 | `d9d65d07adfdb5c52f4b1d80e4e1d2b2f2aee80b1159a65f55c95d887c7ca9a3` | 202 |
| tlx_source | 0 | `6b4a1cd639ccc8e127dc2447e001f13d6f1970ebdefe0653b6c6367edf1c8c13` | 0 |

No run ID occurs in more than one original archive. The long-confirmation CSV/JSON members match the tracked repository evidence byte-for-byte and are not counted twice.
The recomputed positive-control, V2-ablation, and TLX wall-time minima, medians, maxima, and totals match `timing_audit.txt` after its four-decimal rounding.

## Positive-control decision

- 1000000 records: mean 1.040278256, median 1.041974638, hierarchical 95% CI [1.028923369, 1.049936761].
- 5000000 records: mean 1.000000000, median 1.000000000, hierarchical 95% CI [1.000000000, 1.000000000].

Frozen 10% decision: **NO_CREDIBLE_HIGH_HEADROOM_CONTROL**.
All 36 rows have zero misses; checksums and workload fingerprints match within each six-fanout seed group.

## Reproducibility status

All six original archives and `timing_audit.txt` were located. The five experiment archives were independently parsed; the TLX source archive contains source provenance rather than benchmark rows. No campaign was rerun and no measurement was changed.
