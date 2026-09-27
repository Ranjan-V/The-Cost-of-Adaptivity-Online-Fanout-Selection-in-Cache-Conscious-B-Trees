"""Retrospective zero-cost oracle from *measured* static variants only.

This is a run-level upper envelope. The separate phase_oracle.py uses measured
phase sidecars for a more optimistic zero-reconfiguration envelope.
"""
import argparse
import csv
from collections import defaultdict
from pathlib import Path


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--raw", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    groups = defaultdict(dict)
    for file in args.raw.rglob("*.csv"):
        if file.name.endswith("partial.csv") or file.name.endswith(".phases.csv"):
            continue
        with file.open(newline="") as stream:
            for row in csv.DictReader(stream):
                if row.get("variant") != "STATIC":
                    continue
                key = tuple(row.get(field, "") for field in ("machine", "source_id", "compiler_version",
                    "compiler_flags", "experiment_family", "records",
                    "operations", "zipf", "reads", "updates", "hot_fraction", "phase_length",
                    "sample_rate", "segments", "seed", "repetition"))
                groups[key][int(row["fanout"])] = float(row["throughput_ops_sec"])
    output = []
    for key, fanouts in groups.items():
        if 64 not in fanouts or len(fanouts) < 2:
            continue
        best = max(fanouts, key=fanouts.get)
        output.append(list(key) + ["ORACLE-ZERO-COST", best, fanouts[best], fanouts[64], fanouts[best] / fanouts[64] - 1.0])
    if not output:
        raise SystemExit("no complete static fanout comparison")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["machine", "source_id", "compiler_version", "compiler_flags",
                         "experiment_family", "records", "operations", "zipf", "reads",
                         "updates", "hot_fraction", "phase_length", "sample_rate", "segments", "seed", "repetition",
                         "variant", "oracle_best_fanout", "oracle_ops_sec", "f64_ops_sec", "headroom_fraction"])
        writer.writerows(output)


if __name__ == "__main__":
    main()
