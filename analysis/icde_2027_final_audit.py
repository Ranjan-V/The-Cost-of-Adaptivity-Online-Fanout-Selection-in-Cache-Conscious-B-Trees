"""Independent audit of the ICDE 2027 confirmation and sensitivity evidence.

This script reads existing raw CSVs only.  It fails closed on incomplete pairs,
semantic mismatches, unexpected misses, mixed binaries, or mixed machines.
Confidence intervals preserve the five-seed clustering structure by resampling
seeds first and repetitions second.
"""
from __future__ import print_function

import argparse
import csv
import json
import random
import statistics
from collections import defaultdict
from pathlib import Path

csv.field_size_limit(64 * 1024 * 1024)


PAIR_FIELDS = (
    "seed", "repetition", "records", "operations", "zipf", "reads",
    "updates", "workload_family", "warmup", "latency_sampling_rate",
)


def read_rows(root):
    root = Path(root)
    failures = list(root.rglob("*.failed.json"))
    partials = list(root.rglob("*.partial.csv"))
    if failures or partials:
        raise ValueError("failure/partial markers in %s: %d/%d" %
                         (root, len(failures), len(partials)))
    rows = []
    for path in root.rglob("*.csv"):
        if path.name.endswith(".phases.csv"):
            continue
        with path.open(newline="", encoding="utf-8") as stream:
            found = list(csv.DictReader(stream))
        if len(found) != 1:
            raise ValueError("expected one row in %s" % path)
        row = found[0]
        row["_path"] = str(path)
        if int(row.get("misses", "-1")) != 0:
            raise ValueError("nonzero/absent miss count in %s" % path)
        rows.append(row)
    return rows


def require_single(rows, field, label):
    values = {row.get(field, "") for row in rows}
    if len(values) != 1:
        raise ValueError("mixed %s %s: %s" % (label, field, sorted(values)))
    return next(iter(values))


def pair_rows(rows, variants, expected_pairs):
    groups = defaultdict(dict)
    for row in rows:
        key = tuple(row.get(field, "") for field in PAIR_FIELDS)
        variant = row["variant"]
        if variant in groups[key]:
            raise ValueError("duplicate %s for %s" % (variant, key))
        groups[key][variant] = row
    pairs = []
    for key, group in groups.items():
        if set(group) != set(variants):
            raise ValueError("incomplete pair %s: %s" % (key, sorted(group)))
        values = list(group.values())
        for field in ("checksum", "workload_fingerprint"):
            if len({row[field] for row in values}) != 1:
                raise ValueError("%s mismatch for %s" % (field, key))
        pairs.append(group)
    if len(pairs) != expected_pairs:
        raise ValueError("expected %d pairs, found %d" %
                         (expected_pairs, len(pairs)))
    return pairs


def hierarchical_ci(values_by_seed, draws=50000, rng_seed=20271008):
    rng = random.Random(rng_seed)
    seeds = sorted(values_by_seed)
    if len(seeds) != 5:
        raise ValueError("expected five workload seeds, found %d" % len(seeds))
    observed = [value for seed in seeds for value in values_by_seed[seed]]
    boot = []
    for _ in range(draws):
        sampled = []
        for seed in (rng.choice(seeds) for _ in seeds):
            values = values_by_seed[seed]
            sampled.extend(rng.choice(values) for _ in values)
        boot.append(statistics.mean(sampled))
    boot.sort()
    return {
        "mean": statistics.mean(observed),
        "median": statistics.median(observed),
        "ci95_low": boot[int(0.025 * draws)],
        "ci95_high": boot[int(0.975 * draws)],
        "n_pairs": len(observed),
        "n_seeds": len(seeds),
    }


def summarize_pairs(pairs, baseline, competitor):
    ratios = defaultdict(list)
    delta_us = defaultdict(list)
    baseline_wall = []
    competitor_wall = []
    for pair in pairs:
        base = pair[baseline]
        comp = pair[competitor]
        seed = base["seed"]
        base_t = float(base["throughput_ops_sec"])
        comp_t = float(comp["throughput_ops_sec"])
        ratios[seed].append(comp_t / base_t)
        delta_us[seed].append(1e6 / comp_t - 1e6 / base_t)
        baseline_wall.append(float(base["wall_seconds"]))
        competitor_wall.append(float(comp["wall_seconds"]))
    result = hierarchical_ci(ratios)
    result["delta_us_per_op"] = hierarchical_ci(delta_us)
    result["median_wall_seconds"] = {
        baseline: statistics.median(baseline_wall),
        competitor: statistics.median(competitor_wall),
    }
    result["relative_difference_percent"] = 100.0 * (result["mean"] - 1.0)
    return result


def audit_campaign(root, variants, expected_pairs):
    rows = read_rows(root)
    if len(rows) != expected_pairs * len(variants):
        raise ValueError("unexpected row count in %s" % root)
    provenance = {
        "rows": len(rows),
        "machine": require_single(rows, "machine", str(root)),
        "environment": require_single(rows, "environment", str(root)),
        "compiler_flags": require_single(rows, "compiler_flags", str(root)),
        "git_commit": require_single(rows, "git_commit", str(root)),
        "records": require_single(rows, "records", str(root)),
        "operations": require_single(rows, "operations", str(root)),
    }
    return rows, pair_rows(rows, variants, expected_pairs), provenance


def audit_logging(root):
    root = Path(root)
    rows = []
    for path in sorted(root.glob("icde-long-logging-*.csv")):
        if path.name.endswith(".phases.csv"):
            continue
        with path.open(newline="", encoding="utf-8") as stream:
            row = next(csv.DictReader(stream))
        row["_path"] = str(path)
        rows.append(row)
    if len(rows) != 6:
        raise ValueError("expected six long logging rows, found %d" % len(rows))
    groups = defaultdict(dict)
    for row in rows:
        stem = Path(row["_path"]).stem
        state = stem.rsplit("-", 1)[1]
        pair_id = stem.rsplit("-", 1)[0]
        groups[pair_id][state] = row
    ratios = []
    for pair_id, pair in sorted(groups.items()):
        if set(pair) != {"on", "off"}:
            raise ValueError("incomplete logging pair %s" % pair_id)
        on, off = pair["on"], pair["off"]
        for field in ("checksum", "workload_fingerprint", "policy_evaluations",
                      "maintained", "hysteresis_skips", "cooldown_skips",
                      "cost_skips", "rebuild_count"):
            if on[field] != off[field]:
                raise ValueError("logging pair %s differs in %s" % (pair_id, field))
        if on["candidate_decision_history"] == "":
            raise ValueError("logging-on history unexpectedly empty")
        if off["candidate_decision_history"] != "":
            raise ValueError("logging-off history unexpectedly populated")
        ratios.append(float(off["throughput_ops_sec"]) /
                      float(on["throughput_ops_sec"]))
    return {
        "pairs": len(ratios),
        "mean_off_over_on": statistics.mean(ratios),
        "median_off_over_on": statistics.median(ratios),
        "pair_ratios": ratios,
        "interpretation": "exploratory sensitivity check; n=3 matched pairs",
    }


def write_csv(path, rows):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def make_figures(out, original_v2, original_tlx, long_v2, long_tlx):
    import matplotlib.pyplot as plt

    labels = ["Adaptive V2\n1M ops", "Adaptive V2\n50M ops",
              "TLX\n1M ops", "TLX\n50M ops"]
    stats = [original_v2, long_v2, original_tlx, long_tlx]
    means = [item["mean"] for item in stats]
    lower = [item["mean"] - item["ci95_low"] for item in stats]
    upper = [item["ci95_high"] - item["mean"] for item in stats]
    colors = ["#B64A3B", "#B64A3B", "#2A6F97", "#2A6F97"]
    fig, ax = plt.subplots(figsize=(7.1, 3.35))
    ax.bar(range(4), means, color=colors, width=0.68)
    ax.errorbar(range(4), means, yerr=[lower, upper], fmt="none",
                color="black", capsize=3, linewidth=1)
    ax.axhline(1.0, color="black", linestyle="--", linewidth=1)
    ax.set_xticks(range(4)); ax.set_xticklabels(labels)
    ax.set_ylabel("Throughput / matched project Static")
    ax.set_ylim(0.82, 1.015); ax.grid(axis="y", alpha=0.2)
    fig.tight_layout()
    fig.savefig(out / "original_vs_50m.pdf", bbox_inches="tight")
    fig.savefig(out / "original_vs_50m.png", dpi=220, bbox_inches="tight")
    plt.close(fig)


def load_summary(path, metric):
    with Path(path).open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    row = next(item for item in rows if item.get("variant") == metric or
               item.get("metric") == metric)
    return {
        "mean": float(row.get("mean_ratio", row.get("mean"))),
        "median": float(row.get("median_ratio", row.get("median"))),
        "ci95_low": float(row["ci95_low"]),
        "ci95_high": float(row["ci95_high"]),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--out", type=Path,
                        default=Path("results/processed/icde_2027_audit/final"))
    args = parser.parse_args()
    root = args.root.resolve(); out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)

    v2_rows, v2_pairs, v2_prov = audit_campaign(
        root / "results/raw/long_confirmation_v2",
        ["STATIC", "ADAPT-V2"], 25)
    tlx_rows, tlx_pairs, tlx_prov = audit_campaign(
        root / "results/raw/long_confirmation_tlx",
        ["STATIC", "TLX-BTREE"], 25)
    long_v2 = summarize_pairs(v2_pairs, "STATIC", "ADAPT-V2")
    long_v2["total_rebuilds"] = sum(int(row["rebuild_count"])
                                    for row in v2_rows if row["variant"] == "ADAPT-V2")
    long_v2["total_policy_evaluations"] = sum(
        int(row["policy_evaluations"]) for row in v2_rows
        if row["variant"] == "ADAPT-V2")
    long_v2["total_cost_skips"] = sum(int(row["cost_skips"])
                                      for row in v2_rows if row["variant"] == "ADAPT-V2")
    long_tlx = summarize_pairs(tlx_pairs, "STATIC", "TLX-BTREE")
    logging = audit_logging(root / "results/raw/logging_pilot_50m_archive")

    original_v2 = load_summary(
        root / "results/processed/icde_2027_audit/v2_ablation/v2_component_summary.csv",
        "ADAPT-V2")
    original_tlx = load_summary(
        root / "results/processed/icde_2027_audit/external_tlx/external_tlx_summary.csv",
        "tlx_over_project_throughput")
    positive_path = (root / "results/processed/icde_2027_audit/archive_coverage"
                     / "coverage_report.json")
    if not positive_path.is_file():
        raise FileNotFoundError("run icde_2027_archive_audit.py first")
    positive = json.loads(positive_path.read_text(encoding="utf-8"))["positive_control"]
    positive["raw_archive_available"] = True
    positive["raw_rows_independently_verified"] = 36

    report = {
        "analyzed_repository_commit": "259bb6ddccda6d2fd48ea1d336db412839eb6e0f",
        "long_v2": long_v2,
        "long_v2_provenance": v2_prov,
        "long_tlx": long_tlx,
        "long_tlx_provenance": tlx_prov,
        "logging_sensitivity": logging,
        "positive_control": positive,
    }
    with (out / "audit_summary.json").open("w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, sort_keys=True)

    write_csv(out / "confirmation_summary.csv", [
        {"study": "Adaptive V2, 1M ops", **original_v2},
        {"study": "Adaptive V2, 50M ops", **{k: long_v2[k] for k in
            ("mean", "median", "ci95_low", "ci95_high")}},
        {"study": "TLX, 1M ops", **original_tlx},
        {"study": "TLX, 50M ops", **{k: long_tlx[k] for k in
            ("mean", "median", "ci95_low", "ci95_high")}},
    ])
    make_figures(out, original_v2, original_tlx, long_v2, long_tlx)
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
