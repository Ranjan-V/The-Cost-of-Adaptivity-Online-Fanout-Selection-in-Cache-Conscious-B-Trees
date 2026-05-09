#!/usr/bin/env python3
"""Summarize the final clean overhead ablation benchmark."""

import csv
import os
from collections import defaultdict


RESULT_PATH = os.path.join("results", "overhead_summary.csv")

ORDER = [
    "Static B+Tree",
    "Static Region",
    "Static Region + Monitor",
    "Segmented No Records",
    "Segmented Records Only",
    "Segmented Monitor + Records",
    "Segmented Full Adaptive",
]


def load_rows(path):
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle))


def to_float(row, key):
    try:
        return float(row[key])
    except (KeyError, TypeError, ValueError):
        return 0.0


def by_workload(rows):
    grouped = defaultdict(dict)
    for row in rows:
        grouped[row["workload"]][row["index"]] = row
    return grouped


def ratio(row, baseline):
    if not row or not baseline:
        return 0.0
    value = to_float(row, "throughput_ops_sec")
    base = to_float(baseline, "throughput_ops_sec")
    return value / base if base > 0.0 else 0.0


def slowdown(row, baseline):
    return (1.0 - ratio(row, baseline)) * 100.0


def print_row(label, row, static_base, region_base=None):
    ops = to_float(row, "throughput_ops_sec")
    vs_static = ratio(row, static_base)
    vs_region = ratio(row, region_base) if region_base else 0.0
    p95 = to_float(row, "p95_sample_us")
    fanout = row.get("final_fanout", "0")
    adaptations = row.get("adaptations", "0")
    rebuilds = row.get("restructures", "0")
    skipped_cost = row.get("skipped_cost", "0")

    print(
        f"{label:<30}"
        f"{ops:>12.0f}"
        f"{vs_static:>12.2f}"
        f"{vs_region:>12.2f}"
        f"{p95:>10.3f}"
        f"{fanout:>9}"
        f"{adaptations:>8}"
        f"{rebuilds:>8}"
        f"{skipped_cost:>10}"
    )


def main():
    if not os.path.exists(RESULT_PATH):
        raise SystemExit(
            "Missing results/overhead_summary.csv. Run "
            ".\\build\\bench_overhead.exe first."
        )

    grouped = by_workload(load_rows(RESULT_PATH))

    print("\nFinal Clean Overhead Ablation")
    print("=" * 104)

    for workload in sorted(grouped):
        rows = grouped[workload]
        static_base = rows.get("Static B+Tree")
        region_base = rows.get("Static Region")
        if not static_base:
            continue

        print(f"\n{workload}")
        print("-" * 104)
        print(
            f"{'Index':<30}"
            f"{'ops/sec':>12}"
            f"{'vs static':>12}"
            f"{'vs region':>12}"
            f"{'p95 us':>10}"
            f"{'fanout':>9}"
            f"{'adapt':>8}"
            f"{'rebuild':>8}"
            f"{'costskip':>10}"
        )

        for name in ORDER:
            if name in rows:
                print_row(name, rows[name], static_base, region_base)

        static_region = rows.get("Static Region")
        region_monitor = rows.get("Static Region + Monitor")
        segmented_no_records = rows.get("Segmented No Records")
        segmented_records = rows.get("Segmented Records Only")
        segmented_monitor_records = rows.get("Segmented Monitor + Records")
        segmented_full = rows.get("Segmented Full Adaptive")

        print("\nIncremental cost breakdown:")
        if static_region:
            print(
                "  Region routing cost vs Static B+Tree: "
                f"{slowdown(static_region, static_base):.1f}%"
            )
        if region_monitor and static_region:
            print(
                "  Sampled monitor cost on Static Region: "
                f"{slowdown(region_monitor, static_region):.1f}%"
            )
        if segmented_no_records and static_region:
            print(
                "  Segmented object cost vs Static Region: "
                f"{slowdown(segmented_no_records, static_region):.1f}%"
            )
        if segmented_records and segmented_no_records:
            print(
                "  Records metadata cost: "
                f"{slowdown(segmented_records, segmented_no_records):.1f}%"
            )
        if segmented_monitor_records and segmented_records:
            print(
                "  Monitor cost after records metadata: "
                f"{slowdown(segmented_monitor_records, segmented_records):.1f}%"
            )
        if segmented_full and segmented_monitor_records:
            print(
                "  Adaptive decision/rebuild cost: "
                f"{slowdown(segmented_full, segmented_monitor_records):.1f}%"
            )
            print(
                "  Cost-gate rejected rebuilds: "
                f"{segmented_full.get('skipped_cost', '0')}"
            )

    print("\nNotes:")
    print("  vs region uses Static Region as the denominator.")
    print("  Negative costs mean that run-to-run noise or cache placement made the later row faster.")
    print("  Segmented Full Adaptive includes records, sampled monitoring, policy checks, and rebuilds.")


if __name__ == "__main__":
    main()
