"""Verify and summarize the frozen V2 component ladder; never reads historical campaigns."""
import argparse
from collections import defaultdict
from pathlib import Path
from icde_eab_common import exact_groups, hierarchical_ci, read_runs, write_csv

VARIANTS = ["STATIC", "STATIC-REGIONAL", "V2-DATAPATH", "V2-MONITOR", "V2-CONTROLLER", "ADAPT-V2"]


def main():
    p = argparse.ArgumentParser(); p.add_argument("--raw", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True); args = p.parse_args()
    rows = read_runs(args.raw); groups = exact_groups(rows, VARIANTS)
    if len(groups) != 25:
        raise ValueError("expected 25 exact six-way groups, found %d" % len(groups))
    summary = []
    for variant in VARIANTS:
        ratios, deltas = defaultdict(list), defaultdict(list)
        for group in groups:
            seed = group[variant]["seed"]
            tv = float(group[variant]["throughput_ops_sec"]); ts = float(group["STATIC"]["throughput_ops_sec"])
            ratios[seed].append(tv / ts)
            deltas[seed].append(1e6 / tv - 1e6 / ts)
        mean, median, low, high = hierarchical_ci(ratios)
        dmean, _, dlow, dhigh = hierarchical_ci(deltas)
        summary.append({"variant": variant, "pairs": 25, "mean_ratio": mean,
            "median_ratio": median, "ci95_low": low, "ci95_high": high,
            "delta_us_per_op": dmean, "delta_ci_low": dlow, "delta_ci_high": dhigh})
    write_csv(args.out / "v2_component_summary.csv", summary)
    diagnostics = []
    for row in rows:
        diagnostics.append({k: row.get(k, "UNSUPPORTED") for k in
            ("run_id", "variant", "seed", "repetition", "routing_events", "monitor_events",
             "monitor_samples", "monitor_work_ms", "policy_evaluations", "maintained",
             "hysteresis_skips", "cooldown_skips", "cost_skips", "rebuilds_suppressed",
             "rebuild_count", "rebuild_total_ms", "monitor_bytes", "tree_bytes", "peak_rss_bytes")})
    write_csv(args.out / "v2_component_diagnostics.csv", diagnostics)
    import matplotlib.pyplot as plt
    labels = [r["variant"] for r in summary]; means = [float(r["mean_ratio"]) for r in summary]
    errors = [[means[i]-float(summary[i]["ci95_low"]) for i in range(len(means))],
              [float(summary[i]["ci95_high"])-means[i] for i in range(len(means))]]
    fig, ax = plt.subplots(figsize=(7.2, 3.6)); ax.errorbar(range(len(labels)), means, yerr=errors, fmt="o-")
    ax.axhline(1.0, color="black", linestyle="--", linewidth=1); ax.set_xticks(range(len(labels)))
    ax.set_xticklabels(labels, rotation=25, ha="right"); ax.set_ylabel("Throughput / Static")
    fig.tight_layout(); fig.savefig(args.out / "v2_component_ablation.pdf")


if __name__ == "__main__": main()
