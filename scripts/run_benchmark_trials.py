#!/usr/bin/env python3
"""Run repeated benchmark trials and summarize mean/stddev results.

This script is intentionally dependency-free so it works in the current
Windows + Python environment. It appends the per-trial raw rows to CSV files
and prints compact statistical summaries for YCSB, shifting, and overhead
ablation workloads.
"""

import csv
import math
import os
import subprocess
import sys


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
RESULTS_DIR = os.path.join(ROOT, "results")


def mean(values):
    return sum(values) / len(values) if values else 0.0


def stddev(values):
    if len(values) < 2:
        return 0.0
    avg = mean(values)
    variance = sum((value - avg) ** 2 for value in values) / (len(values) - 1)
    return math.sqrt(variance)


def t_critical_95(sample_count):
    # Two-sided 95% Student-t critical values by degrees of freedom.
    table = {
        1: 12.706,
        2: 4.303,
        3: 3.182,
        4: 2.776,
        5: 2.571,
        6: 2.447,
        7: 2.365,
        8: 2.306,
        9: 2.262,
        10: 2.228,
        11: 2.201,
        12: 2.179,
        13: 2.160,
        14: 2.145,
        15: 2.131,
        16: 2.120,
        17: 2.110,
        18: 2.101,
        19: 2.093,
        20: 2.086,
        21: 2.080,
        22: 2.074,
        23: 2.069,
        24: 2.064,
        25: 2.060,
        26: 2.056,
        27: 2.052,
        28: 2.048,
        29: 2.045,
        30: 2.042,
    }
    if sample_count < 2:
        return 0.0
    degrees = sample_count - 1
    return table.get(degrees, 1.960)


def ci95(values):
    if len(values) < 2:
        return 0.0
    return t_critical_95(len(values)) * stddev(values) / math.sqrt(len(values))


def as_float(row, key):
    try:
        return float(row[key])
    except (KeyError, ValueError):
        return 0.0


def first_float(row, keys):
    for key in keys:
        value = as_float(row, key)
        if value != 0.0:
            return value
    return 0.0


def read_csv(path):
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle))


def append_rows(path, rows, trial):
    if not rows:
        return

    fieldnames = ["trial"] + list(rows[0].keys())
    exists = os.path.exists(path)
    with open(path, "a", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        if not exists:
            writer.writeheader()
        for row in rows:
            out = {"trial": trial}
            out.update(row)
            writer.writerow(out)


def run_command(command):
    print("\n$", " ".join(command))
    completed = subprocess.call(command, cwd=ROOT)
    if completed != 0:
        raise RuntimeError("command failed with exit code {}".format(completed))


def build_summary_rows(rows, group_keys, baseline_name):
    groups = {}
    for row in rows:
        key = tuple(row[item] for item in group_keys)
        groups.setdefault(key, []).append(row)

    parent_key_count = len(group_keys) - 1
    parents = sorted(set(key[:parent_key_count] for key in groups.keys()))

    summary_rows = []
    for parent in parents:
        baseline_key = parent + (baseline_name,)
        baseline_rows = groups.get(baseline_key, [])
        baseline_tput = mean([as_float(row, "throughput_ops_sec") for row in baseline_rows])

        for key, items in groups.items():
            if key[:parent_key_count] == parent:
                tputs = [as_float(row, "throughput_ops_sec") for row in items]
                p95s = [first_float(row, ("p95_us", "p95_sample_us")) for row in items]
                p99s = [as_float(row, "p99_us") for row in items]
                avg = mean(tputs)
                sd = stddev(tputs)
                ci = ci95(tputs)
                cv = (sd / avg * 100.0) if avg > 0 else 0.0
                ci_pct = (ci / avg * 100.0) if avg > 0 else 0.0
                vs_base = (avg / baseline_tput) if baseline_tput > 0 else 0.0
                out = {
                    "index": key[-1],
                    "mean_ops_sec": avg,
                    "stddev_ops_sec": sd,
                    "ci95_ops_sec": ci,
                    "cv_percent": cv,
                    "ci95_percent": ci_pct,
                    "vs_baseline": vs_base,
                    "mean_p95_us": mean(p95s),
                    "mean_p99_us": mean(p99s),
                    "trials": len(tputs),
                }
                for metric in (
                    "adaptations",
                    "restructures",
                    "monitored_nodes",
                    "skipped_hysteresis",
                    "skipped_cost",
                    "skipped_cooldown",
                    "final_fanout",
                ):
                    out["mean_" + metric] = mean([as_float(row, metric) for row in items])
                for i, name in enumerate(group_keys[:-1]):
                    out[name] = parent[i]
                summary_rows.append(out)

    return summary_rows


def print_summary(summary_rows, group_keys):
    parent_keys = group_keys[:-1]
    parents = sorted(set(tuple(row[key] for key in parent_keys) for row in summary_rows))

    for parent in parents:
        title = " / ".join(parent) if parent else "All"
        print("\n{}".format(title))
        print("-" * 112)
        print("{:<26} {:>12} {:>11} {:>11} {:>9} {:>9} {:>7}".format(
            "Index", "mean ops/s", "stddev", "95% CI", "cv%", "vs base", "n"))

        rows_for_parent = [
            row for row in summary_rows
            if tuple(row[key] for key in parent_keys) == parent
        ]

        for name, avg, sd, ci, cv, vs_base, count in sorted(
            [
                (
                    row["index"],
                    row["mean_ops_sec"],
                    row["stddev_ops_sec"],
                    row["ci95_ops_sec"],
                    row["cv_percent"],
                    row["vs_baseline"],
                    row["trials"],
                )
                for row in rows_for_parent
            ],
            key=lambda item: item[1],
            reverse=True,
        ):
            print("{:<26} {:>12.0f} {:>11.0f} {:>11.0f} {:>8.1f}% {:>9.2f} {:>7}".format(
                name, avg, sd, ci, cv, vs_base, count))


def write_summary_csv(path, rows, group_keys):
    if not rows:
        return

    parent_keys = list(group_keys[:-1])
    fieldnames = parent_keys + [
        "index",
        "mean_ops_sec",
        "stddev_ops_sec",
        "ci95_ops_sec",
        "cv_percent",
        "ci95_percent",
        "vs_baseline",
        "mean_p95_us",
        "mean_p99_us",
        "mean_adaptations",
        "mean_restructures",
        "mean_monitored_nodes",
        "mean_skipped_hysteresis",
        "mean_skipped_cost",
        "mean_skipped_cooldown",
        "mean_final_fanout",
        "trials",
    ]

    with open(path, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def main():
    if len(sys.argv) > 1 and sys.argv[1] in ("-h", "--help"):
        print("Usage: run_benchmark_trials.py [trials] [records] [ycsb_ops] [theta] [adapt_interval] [shifting_ops_per_phase] [overhead_ops]")
        return 0

    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    records = sys.argv[2] if len(sys.argv) > 2 else "100000"
    ycsb_operations = sys.argv[3] if len(sys.argv) > 3 else "100000"
    theta = sys.argv[4] if len(sys.argv) > 4 else "0.99"
    adapt_interval = sys.argv[5] if len(sys.argv) > 5 else "5000"
    shifting_operations = sys.argv[6] if len(sys.argv) > 6 else ycsb_operations
    overhead_operations = sys.argv[7] if len(sys.argv) > 7 else ycsb_operations

    if not os.path.isdir(RESULTS_DIR):
        os.makedirs(RESULTS_DIR)

    ycsb_trials = os.path.join(RESULTS_DIR, "ycsb_trials.csv")
    shifting_trials = os.path.join(RESULTS_DIR, "shifting_trials.csv")
    overhead_trials = os.path.join(RESULTS_DIR, "overhead_trials.csv")
    ycsb_summary = os.path.join(RESULTS_DIR, "ycsb_trial_summary.csv")
    shifting_summary = os.path.join(RESULTS_DIR, "shifting_trial_summary.csv")
    overhead_summary = os.path.join(RESULTS_DIR, "overhead_trial_summary.csv")

    for path in (
        ycsb_trials,
        shifting_trials,
        overhead_trials,
        ycsb_summary,
        shifting_summary,
        overhead_summary,
    ):
        if os.path.exists(path):
            os.remove(path)

    ycsb_exe = os.path.join(ROOT, "build", "bench_ycsb.exe")
    shifting_exe = os.path.join(ROOT, "build", "bench_shifting.exe")
    overhead_exe = os.path.join(ROOT, "build", "bench_overhead.exe")

    for trial in range(1, trials + 1):
        print("\nTrial {}/{}".format(trial, trials))
        print("=" * 96)

        run_command([ycsb_exe, records, ycsb_operations, theta, adapt_interval])
        append_rows(
            ycsb_trials,
            read_csv(os.path.join(RESULTS_DIR, "ycsb_summary.csv")),
            trial,
        )

        run_command([shifting_exe, records, shifting_operations, theta, adapt_interval])
        append_rows(
            shifting_trials,
            read_csv(os.path.join(RESULTS_DIR, "shifting_summary.csv")),
            trial,
        )

        run_command([overhead_exe, records, overhead_operations, theta, adapt_interval])
        append_rows(
            overhead_trials,
            read_csv(os.path.join(RESULTS_DIR, "overhead_summary.csv")),
            trial,
        )

    print("\n\nYCSB Trial Summary")
    print("=" * 96)
    ycsb_summary_rows = build_summary_rows(
        read_csv(ycsb_trials), ("workload", "index"), "Static B+Tree f=64"
    )
    write_summary_csv(ycsb_summary, ycsb_summary_rows, ("workload", "index"))
    print_summary(ycsb_summary_rows, ("workload", "index"))

    print("\n\nShifting Trial Summary")
    print("=" * 96)
    shifting_summary_rows = build_summary_rows(
        read_csv(shifting_trials), ("phase", "index"), "Static B+Tree f=64"
    )
    write_summary_csv(shifting_summary, shifting_summary_rows, ("phase", "index"))
    print_summary(shifting_summary_rows, ("phase", "index"))

    print("\n\nOverhead Ablation Trial Summary")
    print("=" * 96)
    overhead_summary_rows = build_summary_rows(
        read_csv(overhead_trials), ("workload", "index"), "Static B+Tree"
    )
    write_summary_csv(overhead_summary, overhead_summary_rows, ("workload", "index"))
    print_summary(overhead_summary_rows, ("workload", "index"))

    print("\nRaw trial CSVs:")
    print("  {}".format(os.path.relpath(ycsb_trials, ROOT)))
    print("  {}".format(os.path.relpath(shifting_trials, ROOT)))
    print("  {}".format(os.path.relpath(overhead_trials, ROOT)))
    print("Summary CSVs:")
    print("  {}".format(os.path.relpath(ycsb_summary, ROOT)))
    print("  {}".format(os.path.relpath(shifting_summary, ROOT)))
    print("  {}".format(os.path.relpath(overhead_summary, ROOT)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
