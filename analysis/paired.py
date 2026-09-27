"""Paired ratios by seed/repetition; no independent-sample ratio shortcuts."""
import argparse
import csv
import math
import random
import statistics
from collections import defaultdict
from pathlib import Path


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--raw", type=Path, required=True)
    p.add_argument("--variant", required=True)
    p.add_argument("--baseline", default="STATIC")
    p.add_argument("--output", type=Path, help="Optional paired-cohort CSV")
    args = p.parse_args()
    groups = defaultdict(dict)
    for file in args.raw.rglob("*.csv"):
        if file.name.endswith("partial.csv") or file.name.endswith(".phases.csv"):
            continue
        with file.open(newline="") as stream:
            for row in csv.DictReader(stream):
                if row["variant"] not in (args.variant, args.baseline):
                    continue
                key = tuple(row.get(k, "") for k in ("machine", "source_id", "compiler_version",
                    "compiler_flags", "experiment_family", "records",
                    "operations", "zipf", "reads", "updates", "hot_fraction", "phase_length",
                    "sample_rate", "segments", "fanout", "seed", "repetition"))
                groups[key][row["variant"]] = row
    cohorts = defaultdict(list)
    for key, pair in groups.items():
        if args.variant in pair and args.baseline in pair:
            a, b = pair[args.variant], pair[args.baseline]
            if a["workload_fingerprint"] != b["workload_fingerprint"]:
                raise ValueError("paired workloads have different fingerprints")
            if a["machine"] != b["machine"]:
                raise ValueError("paired runs on different machines")
            cohorts[key[:-2]].append(float(a["throughput_ops_sec"]) / float(b["throughput_ops_sec"]))
    if not cohorts:
        raise SystemExit("no measured pairs")
    records = []
    key_names = ("machine", "source_id", "compiler_version", "compiler_flags",
                 "experiment_family", "records", "operations", "zipf", "reads",
                 "updates", "hot_fraction", "phase_length", "sample_rate", "segments", "fanout")
    for key, ratios in sorted(cohorts.items()):
        rng = random.Random(2027)
        boot = [statistics.mean(rng.choices(ratios, k=len(ratios))) for _ in range(10000)]
        boot.sort()
        print("cohort", key, "pairs", len(ratios), "mean_ratio", statistics.mean(ratios),
              "median_ratio", statistics.median(ratios),
              "sample_sd", statistics.stdev(ratios) if len(ratios)>1 else math.nan,
              "bootstrap_95", (boot[250], boot[9750]))
        lower, upper = boot[250], boot[9750]
        classification = ("WIN" if lower > 1 else "LOSS" if upper < 1 else "INDETERMINATE") if len(ratios) >= 5 else "INSUFFICIENT_N"
        record = dict(zip(key_names, key))
        record.update(variant=args.variant, baseline=args.baseline, pairs=len(ratios),
                      mean_ratio=statistics.mean(ratios), median_ratio=statistics.median(ratios),
                      sample_sd=statistics.stdev(ratios) if len(ratios) > 1 else "UNSUPPORTED",
                      bootstrap_95_low=lower, bootstrap_95_high=upper,
                      classification=classification)
        records.append(record)
        if len(ratios) < 10:
            print("WARNING: small paired sample")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(key_names) +
                ["variant", "baseline", "pairs", "mean_ratio", "median_ratio",
                 "sample_sd", "bootstrap_95_low", "bootstrap_95_high", "classification"])
            writer.writeheader()
            writer.writerows(records)


if __name__ == "__main__":
    main()
