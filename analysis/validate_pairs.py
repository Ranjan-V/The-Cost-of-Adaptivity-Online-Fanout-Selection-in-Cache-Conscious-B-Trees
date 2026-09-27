"""Fail on mismatched deterministic workloads or read/update semantics."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--raw", type=Path, required=True)
    args = p.parse_args()
    groups = defaultdict(list)
    files = 0
    for file in args.raw.rglob("*.csv"):
        if file.name.endswith("partial.csv") or file.name.endswith(".phases.csv"):
            continue
        with file.open(newline="") as stream:
            rows = list(csv.DictReader(stream))
        if len(rows) != 1:
            raise ValueError("one completed run required per CSV: " + str(file))
        row = rows[0]
        if int(row["misses"]) != 0:
            raise ValueError("lookup miss in " + str(file))
        key = tuple(row[k] for k in ("machine", "source_id", "experiment_family", "records",
                                   "operations", "zipf", "reads", "updates", "hot_fraction",
                                   "phase_length", "seed", "repetition"))
        groups[key].append(row)
        files += 1
    mismatches = 0
    for key, rows in groups.items():
        if len({r["workload_fingerprint"] for r in rows}) != 1:
            print("FINGERPRINT MISMATCH", key)
            mismatches += 1
        if len({r["checksum"] for r in rows}) != 1:
            print("CHECKSUM MISMATCH", key)
            mismatches += 1
    if mismatches:
        raise SystemExit(str(mismatches) + " mismatched groups")
    print("checked", files, "completed run CSVs; groups", len(groups))


if __name__ == "__main__":
    main()
