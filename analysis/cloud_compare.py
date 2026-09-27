"""Compare normalized outcomes across CPU groups without pooling raw ops/s."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--summary", type=Path, required=True)
    args = p.parse_args()
    with args.summary.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    fields = ("machine", "source_id", "compiler_version", "compiler_flags",
              "experiment_family", "records", "operations", "zipf", "reads",
              "updates", "phase_length", "hot_fraction", "sample_rate", "segments", "fanout")
    groups = defaultdict(dict)
    for row in rows:
        key = tuple(row.get(name, "") for name in fields)
        groups[key][row["variant"]] = row
    print(",".join(fields + ("v1_over_static", "v2_over_static")))
    for key, variants in sorted(groups.items()):
        if "STATIC" not in variants:
            continue
        base = float(variants["STATIC"]["mean_ops_sec"])
        v1 = float(variants["ADAPT-V1"]["mean_ops_sec"]) / base if "ADAPT-V1" in variants else "UNSUPPORTED"
        v2 = float(variants["ADAPT-V2"]["mean_ops_sec"]) / base if "ADAPT-V2" in variants else "UNSUPPORTED"
        print(",".join(map(str, key + (v1, v2))))


if __name__ == "__main__":
    main()
