"""Select representative regimes only from measured static CSVs."""
import argparse
import csv
import json
import statistics
from collections import defaultdict
from pathlib import Path


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--raw", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--family", default="dynamic_broad",
                   help="Measured static campaign to select from")
    p.add_argument("--variant", default="STATIC-REGIONAL",
                   choices=("STATIC", "STATIC-REGIONAL"),
                   help="Baseline architecture whose fanout headroom defines regimes")
    args = p.parse_args()
    groups = defaultdict(lambda: defaultdict(dict))
    for file in args.raw.rglob("*.csv"):
        if file.name.endswith("partial.csv") or file.name.endswith(".phases.csv"):
            continue
        with file.open(newline="") as stream:
            for row in csv.DictReader(stream):
                if row.get("variant") != args.variant or row.get("experiment_family") != args.family:
                    continue
                key = (row["machine"], row["source_id"], row["compiler_version"],
                       row["compiler_flags"], row["experiment_family"], row["records"],
                       row["operations"], row["zipf"], row["reads"], row["updates"],
                       row["hot_fraction"], row["phase_length"], row["sample_rate"],
                       row["segments"], row["variant"])
                slot = groups[key][(row["seed"], row["repetition"])]
                fanout = int(row["fanout"])
                if fanout in slot:
                    raise ValueError("duplicate static fanout in one paired seed/repetition")
                slot[fanout] = float(row["throughput_ops_sec"])
    selected = {}
    for key, seeds in groups.items():
        benefits = [(max(v.values()) / v[64] - 1.0) for v in seeds.values()
                    if 64 in v and len(v) >= 2 and v[64] > 0]
        if len(benefits) < 5:
            continue
        h = statistics.median(benefits)
        label = "ZERO_HEADROOM" if h < 0.01 else "BORDERLINE_HEADROOM" if h < 0.05 else "HIGH_HEADROOM"
        item = {"key": key, "median_headroom": h, "seed_count": len(benefits)}
        if label not in selected or abs(h - {"ZERO_HEADROOM": 0, "BORDERLINE_HEADROOM": 0.03,
                                             "HIGH_HEADROOM": 0.10}[label]) < abs(selected[label]["median_headroom"] -
                                                                                   {"ZERO_HEADROOM": 0, "BORDERLINE_HEADROOM": 0.03,
                                                                                    "HIGH_HEADROOM": 0.10}[label]):
            selected[label] = item
    if not selected:
        raise SystemExit("insufficient paired static measurements")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(selected, indent=2))


if __name__ == "__main__":
    main()
