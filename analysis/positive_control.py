"""Verify exact Static/Perfect-V2/Adaptive-V2 trios and join measured phase oracle."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path
from icde_eab_common import exact_groups, hierarchical_ci, read_runs, write_csv


def main():
    p = argparse.ArgumentParser(); p.add_argument("--raw", type=Path, required=True)
    p.add_argument("--oracle", type=Path, required=True); p.add_argument("--out", type=Path, required=True)
    args = p.parse_args(); all_rows = read_runs(args.raw)
    rows = [r for r in all_rows if
        (r["experiment_family"] == "icde_positive_control_evaluation" and
         r["variant"] in ("PERFECT-V2", "ADAPT-V2")) or
        (r["experiment_family"] == "icde_positive_control_oracle_grid" and
         r["variant"] == "STATIC-REGIONAL" and r["fanout"] == "64")]
    for row in rows:
        row["experiment_family"] = "icde_positive_control_joined"
    groups = exact_groups(rows, ["STATIC-REGIONAL", "PERFECT-V2", "ADAPT-V2"])
    if len(groups) != 25: raise ValueError("expected 25 exact trios")
    oracle = {}
    with args.oracle.open(newline="", encoding="utf-8") as stream:
        for r in csv.DictReader(stream):
            key = (r["machine"], r["source_id"], r["seed"], r["repetition"], r["records"], r["operations"])
            if key in oracle: raise ValueError("duplicate oracle row")
            oracle[key] = r
    metrics = {name: defaultdict(list) for name in
               ("oracle_static", "perfect_static", "adaptive_static", "actuation_loss", "online_control_loss")}
    points = []
    for g in groups:
        s, p2, a2 = g["STATIC-REGIONAL"], g["PERFECT-V2"], g["ADAPT-V2"]
        key = (s["machine"], s["source_id"], s["seed"], s["repetition"], s["records"], s["operations"])
        if key not in oracle: raise ValueError("missing exact oracle row: " + str(key))
        ospeed = float(oracle[key]["oracle_zero_cost_ops_sec"]); ss = float(s["throughput_ops_sec"])
        ps = float(p2["throughput_ops_sec"]); ads = float(a2["throughput_ops_sec"]); seed = s["seed"]
        vals = {"oracle_static": ospeed/ss, "perfect_static": ps/ss, "adaptive_static": ads/ss,
                "actuation_loss": ospeed/ss-ps/ss, "online_control_loss": ps/ss-ads/ss}
        for name, value in vals.items(): metrics[name][seed].append(value)
        delta = 1e6/ss - 1e6/ospeed
        recurring = 1e6/ads - 1e6/ss
        amortized = float(p2["rebuild_total_ms"])*1000.0/float(s["operations"])
        points.append({"seed": seed, "repetition": s["repetition"], "opportunity_us_per_op": delta,
                       "adaptive_penalty_us_per_op": recurring, "perfect_rebuild_us_per_op": amortized})
    summary = []
    for name, values in metrics.items():
        mean, median, low, high = hierarchical_ci(values)
        summary.append({"metric": name, "trios": 25, "mean": mean, "median": median,
                        "ci95_low": low, "ci95_high": high})
    write_csv(args.out / "positive_control_summary.csv", summary)
    write_csv(args.out / "opportunity_cost_points.csv", points)
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(4.8, 4.0)); x=[float(r["opportunity_us_per_op"]) for r in points]
    y=[float(r["adaptive_penalty_us_per_op"])+float(r["perfect_rebuild_us_per_op"]) for r in points]
    ax.scatter(x, y, alpha=.65); bound=max(x+y+[.001]); ax.plot([0,bound],[0,bound],"k--",label="cost = opportunity")
    ax.set_xlabel("Available layout benefit (us/op)"); ax.set_ylabel("Recurring + amortized cost (us/op)")
    ax.legend(); fig.tight_layout(); fig.savefig(args.out / "opportunity_vs_cost.pdf")


if __name__ == "__main__": main()
