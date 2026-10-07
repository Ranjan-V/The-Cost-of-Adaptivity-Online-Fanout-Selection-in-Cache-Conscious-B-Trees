"""Paired project-static versus TLX reference analysis."""
import argparse
from collections import defaultdict
from pathlib import Path
from icde_eab_common import exact_groups, hierarchical_ci, read_runs, write_csv


def main():
    p = argparse.ArgumentParser(); p.add_argument("--raw", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True); args = p.parse_args()
    rows = read_runs(args.raw); groups = exact_groups(rows, ["STATIC", "TLX-BTREE"])
    if len(groups) != 25: raise ValueError("expected 25 exact pairs")
    ratios, build_ratios, latency = defaultdict(list), defaultdict(list), defaultdict(list)
    for g in groups:
        seed = g["STATIC"]["seed"]
        s, t = g["STATIC"], g["TLX-BTREE"]
        ratios[seed].append(float(t["throughput_ops_sec"]) / float(s["throughput_ops_sec"]))
        build_ratios[seed].append(float(t["build_seconds"]) / float(s["build_seconds"]))
        latency[seed].append(float(t["mean_sample_latency_us"]) - float(s["mean_sample_latency_us"]))
    out = []
    for metric, values in (("tlx_over_project_throughput", ratios), ("tlx_over_project_build_time", build_ratios),
                           ("tlx_minus_project_sample_latency_us", latency)):
        mean, median, low, high = hierarchical_ci(values)
        out.append({"metric": metric, "pairs": 25, "mean": mean, "median": median,
                    "ci95_low": low, "ci95_high": high})
    write_csv(args.out / "external_tlx_summary.csv", out)
    import matplotlib.pyplot as plt
    mean, _, low, high = hierarchical_ci(ratios)
    fig, ax = plt.subplots(figsize=(4.2, 3.4)); ax.errorbar([0], [mean], yerr=[[mean-low], [high-mean]], fmt="o")
    ax.axhline(1.0, color="black", linestyle="--"); ax.set_xlim(-1, 1); ax.set_xticks([0]); ax.set_xticklabels(["TLX / project"])
    ax.set_ylabel("Paired throughput ratio"); fig.tight_layout(); fig.savefig(args.out / "external_tlx.pdf")


if __name__ == "__main__": main()
