"""Analyze the completed Stage 3 selected-adaptive Kaggle campaign.

The script reads the result ZIP directly, verifies campaign completeness, and
uses seed/repetition-matched ratios.  Bootstrap intervals resample seeds first
and repetitions second so repeated executions of one workload seed are not
treated as unrelated workloads.
"""
import argparse
import csv
import io
import math
import random
import statistics
import zipfile
from collections import defaultdict
from pathlib import Path


REGIMES = {
    "zero": {"family": "dynamic_selected_zero", "zipf": "1.2"},
    "borderline": {"family": "dynamic_selected_borderline", "zipf": "0.99"},
    "high": {"family": "dynamic_selected_high", "zipf": "0.8"},
}


def read_csv(archive, name):
    with archive.open(name) as stream:
        return list(csv.DictReader(io.TextIOWrapper(stream, encoding="utf-8-sig")))


def raw_rows(archive):
    rows = []
    for name in archive.namelist():
        if not name.startswith("raw/kaggle/") or not name.endswith(".csv"):
            continue
        if name.endswith(".phases.csv") or ".partial.csv" in name:
            continue
        rows.extend(read_csv(archive, name))
    return rows


def percentile(values, q):
    values = sorted(values)
    position = (len(values) - 1) * q
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return values[lower]
    return values[lower] + (values[upper] - values[lower]) * (position - lower)


def hierarchical_ci(values_by_seed, iterations=20000):
    seeds = sorted(values_by_seed)
    rng = random.Random(20260921)
    boot = []
    for _ in range(iterations):
        sample = []
        for seed in rng.choices(seeds, k=len(seeds)):
            values = values_by_seed[seed]
            sample.extend(rng.choices(values, k=len(values)))
        boot.append(statistics.mean(sample))
    return percentile(boot, 0.025), percentile(boot, 0.975)


def ordinary_ci(values, iterations=20000):
    rng = random.Random(20260921)
    boot = [statistics.mean(rng.choices(values, k=len(values))) for _ in range(iterations)]
    return percentile(boot, 0.025), percentile(boot, 0.975)


def paired_adaptive(rows, family, sample_rate, variant):
    grouped = defaultdict(dict)
    for row in rows:
        if row.get("experiment_family") != family:
            continue
        if row.get("sample_rate") != str(sample_rate):
            continue
        if row.get("variant") not in ("STATIC-REGIONAL", variant):
            continue
        key = (row["seed"], row["repetition"])
        grouped[key][row["variant"]] = row
    ratios_by_seed = defaultdict(list)
    penalties_by_seed = defaultdict(list)
    for (seed, _), pair in grouped.items():
        if set(pair) != {"STATIC-REGIONAL", variant}:
            continue
        adaptive = pair[variant]
        static = pair["STATIC-REGIONAL"]
        if adaptive["workload_fingerprint"] != static["workload_fingerprint"]:
            raise ValueError("paired workload fingerprints differ")
        a = float(adaptive["throughput_ops_sec"])
        s = float(static["throughput_ops_sec"])
        ratios_by_seed[seed].append(a / s)
        penalties_by_seed[seed].append(1e6 / a - 1e6 / s)
    if sum(map(len, ratios_by_seed.values())) != 50:
        raise ValueError("expected 50 paired adaptive runs")
    ratios = [value for values in ratios_by_seed.values() for value in values]
    penalties = [value for values in penalties_by_seed.values() for value in values]
    ratio_ci = hierarchical_ci(ratios_by_seed)
    penalty_ci = hierarchical_ci(penalties_by_seed)
    return {
        "pairs": len(ratios),
        "ratio": statistics.mean(ratios),
        "ratio_low": ratio_ci[0],
        "ratio_high": ratio_ci[1],
        "penalty_us": statistics.mean(penalties),
        "penalty_us_low": penalty_ci[0],
        "penalty_us_high": penalty_ci[1],
    }


def oracle_headroom(rows, oracle_rows, family, zipf):
    static = {}
    for row in rows:
        if (row.get("experiment_family") == family and
                row.get("variant") == "STATIC-REGIONAL" and
                row.get("sample_rate") == "32" and row.get("repetition") == "0"):
            static[row["seed"]] = float(row["throughput_ops_sec"])
    ratios = []
    benefits = []
    for row in oracle_rows:
        if row.get("zipf") != zipf:
            continue
        seed = row["seed"]
        if seed not in static:
            raise ValueError("oracle row has no matched selected static run")
        oracle = float(row["oracle_zero_cost_ops_sec"])
        ratios.append(oracle / static[seed])
        benefits.append(1e6 / static[seed] - 1e6 / oracle)
    if len(ratios) != 5:
        raise ValueError("expected five seed-matched oracle rows")
    ratio_ci = ordinary_ci(ratios)
    benefit_ci = ordinary_ci(benefits)
    return {
        "pairs": len(ratios),
        "ratio": statistics.mean(ratios),
        "ratio_low": ratio_ci[0],
        "ratio_high": ratio_ci[1],
        "benefit_us": statistics.mean(benefits),
        "benefit_us_low": benefit_ci[0],
        "benefit_us_high": benefit_ci[1],
    }


def write_csv(path, records):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)


def write_tex(path, records):
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        r"\begin{tabular}{lrrrr}",
        r"\toprule",
        r"Regime & Oracle/static & Adapt./static & 95\% CI & Penalty ($\mu$s/op) \\",
        r"\midrule",
    ]
    for row in records:
        label = row["regime"].capitalize()
        lines.append(
            (r"%s & %.3f & %.3f & [%.3f, %.3f] & %.4f \\" %
            (label, row["oracle_ratio"], row["adaptive_ratio"],
             row["adaptive_ratio_ci_low"], row["adaptive_ratio_ci_high"],
             row["adaptive_penalty_us"]))
        )
    lines.extend([r"\bottomrule", r"\end{tabular}"])
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def plot(records, output_dirs):
    import matplotlib.pyplot as plt

    labels = [row["regime"].capitalize() for row in records]
    x = list(range(len(records)))

    fig, ax = plt.subplots(figsize=(6.4, 3.5))
    oracle = [row["oracle_ratio"] for row in records]
    adaptive = [row["adaptive_ratio"] for row in records]
    oracle_err = [[row["oracle_ratio"] - row["oracle_ratio_ci_low"] for row in records],
                  [row["oracle_ratio_ci_high"] - row["oracle_ratio"] for row in records]]
    adaptive_err = [[row["adaptive_ratio"] - row["adaptive_ratio_ci_low"] for row in records],
                    [row["adaptive_ratio_ci_high"] - row["adaptive_ratio"] for row in records]]
    width = 0.35
    ax.bar([v - width / 2 for v in x], oracle, width, yerr=oracle_err,
           capsize=3, label="Zero-cost oracle", color="#3b7f6f")
    ax.bar([v + width / 2 for v in x], adaptive, width, yerr=adaptive_err,
           capsize=3, label="Adaptive V2", color="#b6534b")
    ax.axhline(1.0, color="black", linewidth=0.8)
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_xlim(-0.5, len(records) - 0.5)
    ax.set_ylabel("Throughput / matched static regional")
    ax.set_ylim(0.8, 1.08)
    ax.legend(frameon=False, ncol=2, fontsize=8)
    ax.grid(axis="y", alpha=0.2)
    fig.tight_layout()
    for directory in output_dirs:
        directory.mkdir(parents=True, exist_ok=True)
        fig.savefig(directory / "fig_dynamic_selected_validation.pdf", bbox_inches="tight")
        fig.savefig(directory / "fig_dynamic_selected_validation.png", dpi=220,
                    bbox_inches="tight")
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(6.4, 3.5))
    benefit = [max(0.0, row["oracle_benefit_us"]) for row in records]
    penalty = [row["adaptive_penalty_us"] for row in records]
    ax.bar([v - width / 2 for v in x], benefit, width, label="Oracle benefit",
           color="#3b7f6f")
    ax.bar([v + width / 2 for v in x], penalty, width, label="Adaptive penalty",
           color="#b6534b")
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_xlim(-0.5, len(records) - 0.5)
    ax.set_ylabel("Microseconds per operation")
    ax.legend(frameon=False, ncol=2, fontsize=8)
    ax.grid(axis="y", alpha=0.2)
    fig.tight_layout()
    for directory in output_dirs:
        fig.savefig(directory / "fig_dynamic_selected_costs.pdf", bbox_inches="tight")
        fig.savefig(directory / "fig_dynamic_selected_costs.png", dpi=220,
                    bbox_inches="tight")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--processed-dir", type=Path, required=True)
    parser.add_argument("--table", type=Path, required=True)
    parser.add_argument("--figure-dir", type=Path, action="append", required=True)
    args = parser.parse_args()

    with zipfile.ZipFile(args.archive) as archive:
        names = archive.namelist()
        failures = [name for name in names if name.endswith(".failed.json")]
        partials = [name for name in names if ".partial.csv" in name]
        main_csvs = [name for name in names if name.startswith("raw/kaggle/") and
                     name.endswith(".csv") and not name.endswith(".phases.csv") and
                     ".partial.csv" not in name]
        phase_csvs = [name for name in names if name.startswith("raw/kaggle/") and
                      name.endswith(".phases.csv")]
        if failures or partials or len(main_csvs) != 1740 or len(phase_csvs) != 1740:
            raise ValueError("campaign archive is incomplete")
        rows = raw_rows(archive)
        oracle_rows = read_csv(archive, "processed/dynamic_selected_phase_oracle.csv")
        summary_rows = read_csv(archive, "processed/dynamic_selected_summary.csv")

    records = []
    paired_records = []
    for regime, config in REGIMES.items():
        candidates = []
        for sample_rate in (16, 32, 64):
            for variant in ("ADAPT-V1", "ADAPT-V2"):
                measured = paired_adaptive(rows, config["family"], sample_rate, variant)
                paired_records.append(dict(regime=regime, sample_rate=sample_rate,
                                           variant=variant, **measured))
                if variant == "ADAPT-V2":
                    candidates.append((measured["ratio"], sample_rate, measured))
        _, sample_rate, adaptive = max(candidates)
        oracle = oracle_headroom(rows, oracle_rows, config["family"], config["zipf"])
        records.append({
            "regime": regime,
            "best_sample_rate": sample_rate,
            "adaptive_pairs": adaptive["pairs"],
            "adaptive_ratio": adaptive["ratio"],
            "adaptive_ratio_ci_low": adaptive["ratio_low"],
            "adaptive_ratio_ci_high": adaptive["ratio_high"],
            "adaptive_penalty_us": adaptive["penalty_us"],
            "adaptive_penalty_us_ci_low": adaptive["penalty_us_low"],
            "adaptive_penalty_us_ci_high": adaptive["penalty_us_high"],
            "oracle_pairs": oracle["pairs"],
            "oracle_ratio": oracle["ratio"],
            "oracle_ratio_ci_low": oracle["ratio_low"],
            "oracle_ratio_ci_high": oracle["ratio_high"],
            "oracle_benefit_us": oracle["benefit_us"],
            "oracle_benefit_us_ci_low": oracle["benefit_us_low"],
            "oracle_benefit_us_ci_high": oracle["benefit_us_high"],
        })

    args.processed_dir.mkdir(parents=True, exist_ok=True)
    write_csv(args.processed_dir / "dynamic_selected_validation.csv", records)
    write_csv(args.processed_dir / "dynamic_selected_paired.csv", paired_records)
    write_csv(args.processed_dir / "dynamic_selected_summary.csv", summary_rows)
    write_csv(args.processed_dir / "dynamic_selected_phase_oracle.csv", oracle_rows)
    write_tex(args.table, records)
    plot(records, args.figure_dir)
    for row in records:
        print(row)


if __name__ == "__main__":
    main()
