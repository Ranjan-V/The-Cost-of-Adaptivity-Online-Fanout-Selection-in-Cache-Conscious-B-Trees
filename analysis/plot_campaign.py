"""Measured campaign plots; each PDF also gets a PNG sibling."""
import argparse
import csv
import hashlib
import statistics
from collections import defaultdict
from pathlib import Path


def read(path):
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise ValueError("measured input is empty: " + str(path))
    return rows


def save(fig, root, stem):
    fig.tight_layout()
    fig.savefig(root / (stem + ".pdf"))
    fig.savefig(root / (stem + ".png"), dpi=180)


def cohort(row, varying):
    fields = ("machine", "source_id", "compiler", "compiler_version", "compiler_flags",
              "cpu_model", "environment", "experiment_family", "records", "operations",
              "zipf", "reads", "updates", "hot_fraction", "phase_length", "sample_rate",
              "segments", "fanout")
    return tuple((name, row.get(name, "")) for name in fields if name != varying)


def cohort_name(prefix, key):
    digest = hashlib.sha256(repr(key).encode()).hexdigest()[:12]
    return prefix + "_" + digest


ORACLE_PAIR = ("machine", "source_id", "compiler_version", "compiler_flags",
               "records", "operations", "zipf", "reads", "updates", "hot_fraction",
               "phase_length", "sample_rate", "segments", "seed", "repetition")


def oracle_phase_ratios(raw, oracle_csv):
    baselines = {}
    for file in raw.rglob("*.csv"):
        if file.name.endswith("partial.csv") or file.name.endswith(".phases.csv"):
            continue
        with file.open(newline="") as stream:
            for row in csv.DictReader(stream):
                if (row.get("experiment_family") == "phase_length" and
                        row.get("variant") == "STATIC" and row.get("fanout") == "64"):
                    key = tuple(row.get(k, "") for k in ORACLE_PAIR)
                    if key in baselines:
                        raise ValueError("duplicate paired phase-length static baseline")
                    baselines[key] = float(row["throughput_ops_sec"])
    grouped = defaultdict(list)
    for row in read(oracle_csv):
        if row.get("experiment_family") != "phase_length_oracle_grid":
            continue
        key = tuple(row.get(k, "") for k in ORACLE_PAIR)
        if key not in baselines or baselines[key] <= 0:
            raise ValueError("phase oracle missing paired repetition-0 static baseline")
        stratum = key[:-2]  # Drop seed/repetition, retain sample/segment and phase length.
        grouped[stratum].append(float(row["oracle_zero_cost_ops_sec"]) / baselines[key])
    return {key: statistics.median(values) for key, values in grouped.items()}


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--summary", type=Path, required=True)
    p.add_argument("--raw", type=Path, required=True)
    p.add_argument("--phase-oracle", type=Path,
                   help="Measured phase oracle CSV for the zero-cost line")
    p.add_argument("--output-dir", type=Path, required=True)
    args = p.parse_args()
    import matplotlib.pyplot as plt
    summary = read(args.summary)
    oracle_ratios = oracle_phase_ratios(args.raw, args.phase_oracle) if args.phase_oracle else {}
    args.output_dir.mkdir(parents=True, exist_ok=True)

    # Never combine raw throughput from different machine fingerprints.
    by_machine = defaultdict(list)
    for row in summary:
        by_machine[row["machine"]].append(row)
    for machine, rows in by_machine.items():
        phase = defaultdict(lambda: defaultdict(dict))
        scale = defaultdict(lambda: defaultdict(dict))
        for row in rows:
            if row["experiment_family"] == "phase_length":
                phase[cohort(row, "phase_length")][row["phase_length"]][row["variant"]] = row
            if row["experiment_family"] == "scale":
                scale[cohort(row, "records")][row["records"]][row["variant"]] = row
        for group_key, group in phase.items():
            fig, ax = plt.subplots(figsize=(6, 4))
            for variant in ("STATIC", "ADAPT-V1", "ADAPT-V2", "PERFECT-V1", "PERFECT-V2"):
                points = []
                for length, variants in group.items():
                    if "STATIC" in variants and variant in variants:
                        base = float(variants["STATIC"]["mean_ops_sec"])
                        points.append((int(length), float(variants[variant]["mean_ops_sec"]) / base))
                if points:
                    points.sort()
                    ax.plot([x for x, _ in points], [y for _, y in points], marker="o", label=variant)
            selected = dict(group_key)
            oracle_points = []
            for length in group:
                lookup = tuple(length if k == "phase_length" else selected.get(k, "")
                               for k in ORACLE_PAIR[:-2])
                if lookup in oracle_ratios:
                    oracle_points.append((int(length), oracle_ratios[lookup]))
            if oracle_points:
                oracle_points.sort()
                ax.plot([x for x, _ in oracle_points], [y for _, y in oracle_points],
                        marker="s", linestyle="--", label="ORACLE-ZERO-COST (retrospective)")
            ax.axhline(1.0, color="black", linewidth=0.7)
            ax.set_xscale("log")
            ax.set(xlabel="Operations per phase", ylabel="Throughput / static f64",
                   title="Measured phase-length response")
            ax.legend(fontsize=7)
            save(fig, args.output_dir, cohort_name(machine + "_phase_length", group_key))
            plt.close(fig)
        for group_key, group in scale.items():
            fig, ax = plt.subplots(figsize=(6, 4))
            for variant in ("STATIC", "ADAPT-V1", "ADAPT-V2"):
                points = []
                for records, variants in group.items():
                    if "STATIC" in variants and variant in variants:
                        base = float(variants["STATIC"]["mean_ops_sec"])
                        points.append((int(records), float(variants[variant]["mean_ops_sec"]) / base))
                if points:
                    points.sort()
                    ax.plot([x for x, _ in points], [y for _, y in points], marker="o", label=variant)
            ax.axhline(1.0, color="black", linewidth=0.7)
            ax.set(xlabel="Records", ylabel="Throughput / static f64",
                   title="Measured scale response")
            ax.legend(fontsize=7)
            save(fig, args.output_dir, cohort_name(machine + "_scale", group_key))
            plt.close(fig)

    # Monitoring ladder is a package-level comparison. It is not a causal
    # waterfall unless the underlying paired intervals support each contrast.
    ladder = ["STATIC", "STATIC+ROUTER", "STATIC+BOUNDED-MONITOR",
              "STATIC+OLD-MONITOR", "STATIC+OLD-SHADOW-RECORDS",
              "STATIC+BOUNDED-MONITOR+ROUTER", "ADAPT-V2", "ADAPT-V1"]
    for machine, rows in by_machine.items():
        groups = defaultdict(dict)
        for row in rows:
            if row["experiment_family"] == "monitor_ladder":
                groups[cohort(row, "")][row["variant"]] = row
        for key, variants in groups.items():
            if "STATIC" not in variants:
                continue
            baseline = float(variants["STATIC"]["mean_ops_sec"])
            names = [name for name in ladder if name in variants]
            ratios = [float(variants[name]["mean_ops_sec"]) / baseline for name in names]
            fig, ax = plt.subplots(figsize=(8, 4))
            ax.barh(range(len(names)), ratios, color="#497e83")
            ax.axvline(1, color="black", linewidth=0.7)
            ax.set_yticks(range(len(names)))
            ax.set_yticklabels(names, fontsize=7)
            ax.set(xlabel="Throughput / static", title="Measured monitor/routing ladder")
            save(fig, args.output_dir, cohort_name(machine + "_ladder", key))
            plt.close(fig)


if __name__ == "__main__":
    main()
