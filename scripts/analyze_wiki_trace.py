#!/usr/bin/env python3
"""Summarize results/wiki_trace_summary.csv from bench_wiki_trace.exe."""

import csv
import os
import sys


def as_float(row, key):
    try:
        return float(row[key])
    except (KeyError, ValueError):
        return 0.0


def as_int(row, key):
    try:
        return int(float(row[key]))
    except (KeyError, ValueError):
        return 0


def aggregate(rows):
    grouped = {}
    for row in rows:
        name = row["index"]
        if name not in grouped:
            grouped[name] = {
                "index": name,
                "operations": 0,
                "run_ms": 0.0,
                "p95_sum": 0.0,
                "p99_sum": 0.0,
                "count": 0,
                "adaptations": 0,
                "restructures": 0,
                "final_fanout": row.get("final_fanout", "0"),
            }

        item = grouped[name]
        item["operations"] += as_int(row, "operations")
        item["run_ms"] += as_float(row, "run_ms")
        item["p95_sum"] += as_float(row, "p95_us")
        item["p99_sum"] += as_float(row, "p99_us")
        item["count"] += 1
        item["adaptations"] += as_int(row, "adaptations")
        item["restructures"] += as_int(row, "restructures")
        item["final_fanout"] = row.get("final_fanout", item["final_fanout"])

    output = []
    for item in grouped.values():
        run_ms = item["run_ms"]
        item["throughput"] = item["operations"] / run_ms * 1000.0 if run_ms > 0 else 0.0
        item["avg_p95"] = item["p95_sum"] / item["count"] if item["count"] else 0.0
        item["avg_p99"] = item["p99_sum"] / item["count"] if item["count"] else 0.0
        output.append(item)
    return sorted(output, key=lambda row: row["throughput"], reverse=True)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join("results", "wiki_trace_summary.csv")
    if not os.path.exists(path):
        print("Missing CSV:", path)
        print("Run .\\build\\bench_wiki_trace.exe first.")
        return 1

    with open(path, newline="") as handle:
        rows = list(csv.DictReader(handle))

    if not rows:
        print("No rows found in", path)
        return 1

    source = rows[0].get("trace_source", "unknown")
    days = []
    for row in rows:
        if row["day"] not in days:
            days.append(row["day"])

    aggregate_rows = aggregate(rows)
    baseline = next((row for row in aggregate_rows if row["index"] == "Static B+Tree f=64"), None)
    baseline_tput = baseline["throughput"] if baseline else 0.0
    segmented = next((row for row in aggregate_rows if row["index"] == "Segmented Adaptive"), None)
    no_adapt = next((row for row in aggregate_rows if row["index"] == "Segmented No Adapt"), None)

    print("\nWikipedia Trace Summary")
    print("=" * 92)
    print("Trace source: {} | Days: {}".format(source, len(days)))
    print("-" * 92)
    print("{:<24} {:>12} {:>10} {:>10} {:>10} {:>8} {:>8}".format(
        "Index", "ops/sec", "vs f64", "p95 us", "p99 us", "adapt", "rebuild"))

    for row in aggregate_rows:
        speedup = row["throughput"] / baseline_tput if baseline_tput > 0 else 0.0
        print("{:<24} {:>12.0f} {:>10.2f} {:>10.3f} {:>10.3f} {:>8} {:>8}".format(
            row["index"],
            row["throughput"],
            speedup,
            row["avg_p95"],
            row["avg_p99"],
            row["adaptations"],
            row["restructures"]))

    if segmented and baseline:
        print("\nSegmented Adaptive vs Static f64: {:.2f}x".format(
            segmented["throughput"] / baseline_tput if baseline_tput > 0 else 0.0))
    if segmented and no_adapt:
        print("Segmented Adaptive vs Segmented No Adapt: {:.2f}x".format(
            segmented["throughput"] / no_adapt["throughput"] if no_adapt["throughput"] > 0 else 0.0))

    print("\nInterpretation:")
    print("  This benchmark uses changing daily popularity phases from Wikipedia pageviews.")
    print("  If Segmented Adaptive beats Static f64 but not Segmented No Adapt, segmentation is helping more than adaptation.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
