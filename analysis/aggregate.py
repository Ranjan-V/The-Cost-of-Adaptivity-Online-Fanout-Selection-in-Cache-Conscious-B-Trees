"""Aggregate measured per-run CSVs, preserving machine and paired-seed boundaries."""
import argparse
import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path


T95 = {2: 12.706, 3: 4.303, 4: 3.182, 5: 2.776, 6: 2.571,
       7: 2.447, 8: 2.365, 9: 2.306, 10: 2.262, 11: 2.228,
       12: 2.201, 13: 2.179, 14: 2.160, 15: 2.145,
       16: 2.131, 17: 2.120, 18: 2.110, 19: 2.101, 20: 2.093,
       21: 2.086, 22: 2.080, 23: 2.074, 24: 2.069, 25: 2.064,
       26: 2.060, 27: 2.056, 28: 2.052, 29: 2.048, 30: 2.045}
KEYS = ("machine", "source_id", "compiler", "compiler_version", "compiler_flags",
        "cpu_model", "environment", "experiment_family", "records", "operations", "zipf",
        "reads", "updates", "hot_fraction", "phase_length", "sample_rate",
        "segments", "fanout")


def read_rows(root):
    rows = []
    for file in root.rglob("*.csv"):
        if file.name.endswith("partial.csv") or file.name.endswith(".phases.csv"):
            continue
        with file.open(newline="") as stream:
            parsed = list(csv.DictReader(stream))
        if len(parsed) != 1:
            raise ValueError("expected one run per file: " + str(file))
        row = parsed[0]
        row["_file"] = str(file)
        rows.append(row)
    return rows


def summary(values):
    n = len(values)
    mean = statistics.mean(values)
    sd = statistics.stdev(values) if n > 1 else math.nan
    crit = T95.get(n, 1.96 if n >= 30 else 2.0)
    return (mean, statistics.median(values), sd,
            crit * sd / math.sqrt(n) if n > 1 else math.nan,
            sd / mean if n > 1 and mean else math.nan)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--raw", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    rows = read_rows(args.raw)
    if not rows:
        raise SystemExit("no measured raw rows; no summary created")
    groups = defaultdict(list)
    for row in rows:
        if row.get("throughput_ops_sec", "") in ("", "TODO_MEASURE", "NOT_RUN"):
            continue
        key = tuple(row.get(name, "") for name in KEYS) + (row["variant"],)
        groups[key].append(float(row["throughput_ops_sec"]))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="") as stream:
        fields = list(KEYS) + ["variant", "n", "mean_ops_sec", "median_ops_sec",
                               "sample_sd_ops_sec", "ci95_halfwidth_ops_sec", "cv"]
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for key, values in sorted(groups.items()):
            mean, median, sd, ci, cv = summary(values)
            record = dict(zip(fields[:len(key)], key))
            record.update(n=len(values), mean_ops_sec=mean, median_ops_sec=median,
                          sample_sd_ops_sec=sd, ci95_halfwidth_ops_sec=ci, cv=cv)
            writer.writerow(record)
            if len(values) < 5 or (not math.isnan(cv) and cv > 0.10):
                print("WARNING: limited/noisy group", key, "n=", len(values), "cv=", cv)


if __name__ == "__main__":
    main()
