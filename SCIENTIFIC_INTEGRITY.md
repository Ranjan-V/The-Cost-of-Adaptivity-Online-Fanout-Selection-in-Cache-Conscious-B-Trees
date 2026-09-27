# Scientific integrity rules

1. Report no result without its raw output file and exact command/config.
2. Document any excluded run; never delete an inconvenient run silently.
3. Log failed runs separately; a failure is never a zero throughput value.
4. Relabel analysis exploratory when parameters were retuned after final results were inspected.
5. Distinguish tuning workloads from final evaluation workloads.
6. Preserve seeds, run order, configs, source identifiers, and raw logs.
7. Report cloud CPU changes and analyze distinct machine fingerprints separately.
8. Preserve unsupported hardware counters as `UNSUPPORTED`, never zero.
9. Distinguish model projections from observed timings; do not publish predicted values as measured.
10. Generate paper numbers from raw result files programmatically where practical.
11. Do not describe V1/V2 outcomes, cache misses, or oracle headroom until the new campaign is executed and reviewed.
