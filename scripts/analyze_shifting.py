#!/usr/bin/env python3
"""Summarize results/shifting_summary.csv from bench_shifting.exe."""

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
                "reconfigure_ms": 0.0,
                "adaptations": 0,
                "restructures": 0,
                "p95_sum": 0.0,
                "p99_sum": 0.0,
                "count": 0,
            }

        item = grouped[name]
        item["operations"] += as_int(row, "operations")
        item["run_ms"] += as_float(row, "run_ms")
        item["reconfigure_ms"] += as_float(row, "reconfigure_ms")
        item["adaptations"] += as_int(row, "adaptations")
        item["restructures"] += as_int(row, "restructures")
        item["p95_sum"] += as_float(row, "p95_us")
        item["p99_sum"] += as_float(row, "p99_us")
        item["count"] += 1

    for item in grouped.values():
        run_ms = item["run_ms"]
        total_ms = item["run_ms"] + item["reconfigure_ms"]
        item["throughput"] = (item["operations"] / run_ms * 1000.0) if run_ms > 0 else 0.0
        item["throughput_including_reconfig"] = (
            item["operations"] / total_ms * 1000.0
        ) if total_ms > 0 else 0.0
        item["avg_p95"] = item["p95_sum"] / item["count"] if item["count"] else 0.0
        item["avg_p99"] = item["p99_sum"] / item["count"] if item["count"] else 0.0

    return list(grouped.values())


def print_phase_summary(rows):
    phases = []
    for row in rows:
        if row["phase"] not in phases:
            phases.append(row["phase"])

    for phase in phases:
        group = [row for row in rows if row["phase"] == phase]
        baseline = next((row for row in group if row["index"] == "Static B+Tree f=64"), None)
        base_tput = as_float(baseline, "throughput_ops_sec") if baseline else 0.0
        oracle_rows = [row for row in group if row["index"].startswith("Oracle Hot")]
        oracle_best = max(oracle_rows, key=lambda row: as_float(row, "throughput_ops_sec")) if oracle_rows else None

        print("\n{}".format(phase))
        print("-" * 96)
        print("{:<24} {:>12} {:>10} {:>10} {:>10} {:>9} {:>7}".format(
            "Index", "ops/sec", "vs f=64", "incl recfg", "p95 us", "fanout", "rebuild"))

        for row in sorted(group, key=lambda item: as_float(item, "throughput_ops_sec"), reverse=True):
            tput = as_float(row, "throughput_ops_sec")
            speedup = (tput / base_tput) if base_tput > 0 else 0.0
            print("{:<24} {:>12.0f} {:>10.2f} {:>10.0f} {:>10.3f} {:>9} {:>7}".format(
                row["index"],
                tput,
                speedup,
                as_float(row, "throughput_with_reconfig_ops_sec"),
                as_float(row, "p95_us"),
                row["final_fanout"],
                row["restructures"]))

        if oracle_best:
            oracle_speedup = (
                as_float(oracle_best, "throughput_ops_sec") / base_tput
            ) if base_tput > 0 else 0.0
            print("Oracle hot-segment envelope: {} ({:.2f}x vs static f=64)".format(
                oracle_best["index"], oracle_speedup))


def phase_envelope(rows, predicate):
    phases = []
    for row in rows:
        if row["phase"] not in phases:
            phases.append(row["phase"])

    chosen = []
    for phase in phases:
        candidates = [row for row in rows if row["phase"] == phase and predicate(row)]
        if candidates:
            chosen.append(max(candidates, key=lambda row: as_float(row, "throughput_ops_sec")))

    operations = sum(as_int(row, "operations") for row in chosen)
    run_ms = sum(as_float(row, "run_ms") for row in chosen)
    reconfigure_ms = sum(as_float(row, "reconfigure_ms") for row in chosen)
    throughput = operations / run_ms * 1000.0 if run_ms > 0 else 0.0
    throughput_including_reconfig = (
        operations / (run_ms + reconfigure_ms) * 1000.0
    ) if (run_ms + reconfigure_ms) > 0 else 0.0
    return {
        "chosen": chosen,
        "throughput": throughput,
        "throughput_including_reconfig": throughput_including_reconfig,
    }


def print_aggregate_summary(rows):
    aggregate_rows = aggregate(rows)
    baseline = next((row for row in aggregate_rows if row["index"] == "Static B+Tree f=64"), None)
    base_tput = baseline["throughput"] if baseline else 0.0
    oracle_rows = [row for row in aggregate_rows if row["index"].startswith("Oracle Hot")]
    oracle_best = max(oracle_rows, key=lambda row: row["throughput"]) if oracle_rows else None
    adaptive = next((row for row in aggregate_rows if row["index"] == "Segmented Adaptive"), None)
    static_envelope = phase_envelope(rows, lambda row: row["index"].startswith("Static"))
    all_envelope = phase_envelope(rows, lambda row: True)

    print("\nAggregate Across Phases")
    print("=" * 96)
    print("{:<24} {:>12} {:>10} {:>12} {:>10} {:>10} {:>7} {:>7}".format(
        "Index", "ops/sec", "vs f=64", "incl recfg", "avg p95", "avg p99", "adapt", "rebuild"))

    for row in sorted(aggregate_rows, key=lambda item: item["throughput"], reverse=True):
        speedup = row["throughput"] / base_tput if base_tput > 0 else 0.0
        print("{:<24} {:>12.0f} {:>10.2f} {:>12.0f} {:>10.3f} {:>10.3f} {:>7} {:>7}".format(
            row["index"],
            row["throughput"],
            speedup,
            row["throughput_including_reconfig"],
            row["avg_p95"],
            row["avg_p99"],
            row["adaptations"],
            row["restructures"]))

    if oracle_best and baseline:
        headroom = oracle_best["throughput"] / base_tput if base_tput > 0 else 0.0
        print("\nOracle hot-segment headroom: {} at {:.2f}x static f=64.".format(
            oracle_best["index"], headroom))

    if oracle_best and adaptive:
        gap = adaptive["throughput"] / oracle_best["throughput"] if oracle_best["throughput"] > 0 else 0.0
        print("Adaptive/oracle ratio: {:.2f}.".format(gap))
        oracle_has_headroom = base_tput > 0 and oracle_best["throughput"] > base_tput * 1.02
        if not oracle_has_headroom:
            print("Interpretation: the hot-segment oracle does not beat static f=64 here; this is negative-result evidence.")
        elif gap < 0.80:
            print("Interpretation: the workload has oracle headroom, but online metadata/policy cost is still too high.")
        else:
            print("Interpretation: segmented adaptive is close to the oracle envelope on this shifting workload.")

    if baseline:
        static_speedup = static_envelope["throughput"] / base_tput if base_tput > 0 else 0.0
        all_speedup = all_envelope["throughput"] / base_tput if base_tput > 0 else 0.0
        print("\nRetrospective static phase envelope: {:.2f}x static f=64.".format(static_speedup))
        print("Retrospective best-measured phase envelope: {:.2f}x static f=64.".format(all_speedup))
        print("Best measured choice by phase:")
        for row in all_envelope["chosen"]:
            print("  {:<10} -> {:<22} {:>10.0f} ops/sec".format(
                row["phase"], row["index"], as_float(row, "throughput_ops_sec")))


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join("results", "shifting_summary.csv")
    if not os.path.exists(path):
        print("Missing CSV:", path)
        print("Run .\\build\\bench_shifting.exe first.")
        return 1

    with open(path, newline="") as handle:
        rows = list(csv.DictReader(handle))

    if not rows:
        print("No rows found in", path)
        return 1

    print("\nShifting + Oracle Summary")
    print("=" * 96)
    print_phase_summary(rows)
    print_aggregate_summary(rows)

    print("\nNotes:")
    print("  Oracle rows know phase boundaries and the hot segment.")
    print("  'incl recfg' includes phase-boundary oracle rebuild time.")
    print("  If the oracle envelope is not faster than static, the adaptation idea lacks headroom for this workload.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
