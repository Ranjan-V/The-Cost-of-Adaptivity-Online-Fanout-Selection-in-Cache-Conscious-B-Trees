#!/usr/bin/env python3
"""Summarize results/ycsb_summary.csv from bench_ycsb.exe."""

import csv
import os
import sys


def as_float(row, key):
    try:
        return float(row[key])
    except (KeyError, ValueError):
        return 0.0


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join("results", "ycsb_summary.csv")
    if not os.path.exists(path):
        print("Missing CSV:", path)
        print("Run .\\build\\bench_ycsb.exe first.")
        return 1

    with open(path, newline="") as handle:
        rows = list(csv.DictReader(handle))

    if not rows:
        print("No rows found in", path)
        return 1

    workloads = sorted(set(row["workload"] for row in rows))

    print("\nYCSB Summary")
    print("=" * 80)

    for workload in workloads:
        group = [row for row in rows if row["workload"] == workload]
        baseline = next((row for row in group if row["index"] == "Static B+Tree f=64"), None)
        best = max(group, key=lambda row: as_float(row, "throughput_ops_sec"))
        base_tput = as_float(baseline, "throughput_ops_sec") if baseline else 0.0

        print("\n{}".format(workload))
        print("-" * 80)
        print("{:<24} {:>12} {:>10} {:>10} {:>10}".format(
            "Index", "ops/sec", "vs f=64", "p95 us", "p99 us"))

        for row in sorted(group, key=lambda item: as_float(item, "throughput_ops_sec"), reverse=True):
            tput = as_float(row, "throughput_ops_sec")
            speedup = (tput / base_tput) if base_tput > 0 else 0.0
            print("{:<24} {:>12.0f} {:>10.2f} {:>10.3f} {:>10.3f}".format(
                row["index"],
                tput,
                speedup,
                as_float(row, "p95_us"),
                as_float(row, "p99_us")))

        print("Best throughput:", best["index"])

    print("\nInterpretation tip:")
    print("  vs f=64 > 1.00 means faster than the static fanout=64 baseline.")
    print("  For publication-quality claims, use larger runs and repeat trials.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
