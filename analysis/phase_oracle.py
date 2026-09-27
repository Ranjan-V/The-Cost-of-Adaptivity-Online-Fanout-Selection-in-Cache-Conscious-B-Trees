"""Retrospective zero-cost phase envelope from paired STATIC-REGIONAL runs."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path


IDENTITY = ("machine", "source_id", "compiler_version", "compiler_flags", "experiment_family", "seed", "repetition", "records",
            "operations", "zipf", "reads", "updates", "hot_fraction", "phase_length", "sample_rate", "segments")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--raw", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--family", help="Only process this experiment family")
    args = p.parse_args()
    groups = defaultdict(lambda: defaultdict(list))
    for file in args.raw.rglob("*.phases.csv"):
        if "partial" in file.name:
            continue
        with file.open(newline="") as stream:
            for row in csv.DictReader(stream):
                if (row["variant"] != "STATIC-REGIONAL" or
                        (args.family and row.get("experiment_family") != args.family)):
                    continue
                key = tuple(row[k] for k in IDENTITY)
                groups[key][int(row["phase_index"])].append(row)
    output = []
    for key, phases in groups.items():
        if sorted(phases) != list(range(len(phases))):
            raise ValueError("non-contiguous phase indices in regional oracle group")
        selected = []
        for index in sorted(phases):
            candidates = phases[index]
            if (len({int(r["fanout"]) for r in candidates}) < 2 or
                    not any(int(r["fanout"]) == 64 for r in candidates)):
                raise ValueError("phase oracle requires f64 and another measured fanout; "
                                 "use --family to exclude smoke or unrelated campaigns")
            if len({r["phase_fingerprint"] for r in candidates}) != 1:
                raise ValueError("phase operation stream differs within oracle group")
            if len({int(r["phase_operations"]) for r in candidates}) != 1:
                raise ValueError("phase operation counts differ within oracle group")
            best = min(candidates, key=lambda r: float(r["phase_seconds"]))
            selected.append(best)
        if sum(int(r["phase_operations"]) for r in selected) != int(key[IDENTITY.index("operations")]):
            raise ValueError("phase oracle does not cover the full measured workload")
        total_ops = sum(int(r["phase_operations"]) for r in selected)
        total_seconds = sum(float(r["phase_seconds"]) for r in selected)
        output.append(list(key) + ["ORACLE-ZERO-COST", ";".join(r["fanout"] for r in selected),
                                   total_ops, total_seconds, total_ops / total_seconds])
    if not output:
        raise SystemExit("no paired STATIC-REGIONAL phase measurements")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(list(IDENTITY) + ["variant", "phase_fanouts", "oracle_operations",
                                           "oracle_zero_cost_seconds", "oracle_zero_cost_ops_sec"])
        writer.writerows(output)


if __name__ == "__main__":
    main()
