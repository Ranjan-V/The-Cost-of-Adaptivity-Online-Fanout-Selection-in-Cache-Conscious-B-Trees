"""Generate the adaptive-scale figure after validated analysis (not during preparation)."""
import argparse
import csv
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    import matplotlib.pyplot as plt
    with args.summary.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    x = [int(r["records"]) for r in rows]
    y = [float(r["mean_ratio"]) for r in rows]
    lo = [v - float(r["bootstrap_95_low"]) for v, r in zip(y, rows)]
    hi = [float(r["bootstrap_95_high"]) - v for v, r in zip(y, rows)]
    fig, ax = plt.subplots(figsize=(5.0, 2.8))
    ax.errorbar(x, y, yerr=[lo, hi], marker="o", capsize=4, color="#006d77")
    ax.axhline(1.0, color="#333333", linestyle="--", linewidth=1)
    ax.set_xscale("log")
    ax.set_xticks(x, ["100K", "1M", "5M"])
    ax.set_xlabel("Records")
    ax.set_ylabel("Adaptive V2 / Static throughput")
    ax.grid(axis="y", alpha=0.25)
    fig.tight_layout()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, dpi=300, bbox_inches="tight")


if __name__ == "__main__":
    main()
