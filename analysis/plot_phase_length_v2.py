"""Plot the verified phase-length V2 decomposition."""
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

    x = [int(row["phase_length"]) for row in rows]
    series = (
        ("Zero-cost oracle", "oracle_ratio", "#1b7f5a", "o"),
        ("Perfect V2", "perfect_ratio", "#ba3c3c", "s"),
        ("Adaptive V2", "adaptive_ratio", "#2367a8", "^"),
    )

    fig, ax = plt.subplots(figsize=(5.4, 3.1))
    for label, prefix, color, marker in series:
        y = [float(row[prefix + "_mean"]) for row in rows]
        low = [value - float(row[prefix + "_ci_low"])
               for value, row in zip(y, rows)]
        high = [float(row[prefix + "_ci_high"]) - value
                for value, row in zip(y, rows)]
        ax.errorbar(x, y, yerr=[low, high], label=label, color=color,
                    marker=marker, linewidth=1.6, markersize=5, capsize=3)

    ax.axhline(1.0, color="#333333", linestyle="--", linewidth=1)
    ax.set_xscale("log")
    ax.set_xticks(x, ["10K", "100K", "1M", "5M", "10M"])
    ax.set_xlabel("Operations per workload phase")
    ax.set_ylabel("Throughput / matched static")
    ax.set_ylim(0.0, 1.18)
    ax.grid(axis="y", alpha=0.25)
    ax.legend(frameon=False, ncol=1, loc="lower right")
    fig.tight_layout()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, dpi=300, bbox_inches="tight")


if __name__ == "__main__":
    main()
