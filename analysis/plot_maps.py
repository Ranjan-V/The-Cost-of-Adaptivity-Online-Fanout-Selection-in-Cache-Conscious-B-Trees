"""Measured opportunity heatmaps and CI-based adaptation classification maps."""
import argparse
import csv
import hashlib
import statistics
from collections import defaultdict
from pathlib import Path


def rows(path):
    with path.open(newline="") as stream:
        result = list(csv.DictReader(stream))
    if not result:
        raise ValueError("measured input is empty: " + str(path))
    return result


def slug(key):
    return hashlib.sha256(repr(key).encode()).hexdigest()[:12]


def save(fig, folder, stem):
    fig.tight_layout()
    fig.savefig(folder / (stem + ".pdf"))
    fig.savefig(folder / (stem + ".png"), dpi=180)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--oracle", type=Path, required=True)
    p.add_argument("--summary", type=Path, required=True)
    p.add_argument("--paired", type=Path, help="Paired V2/static CSV; required for break-even map")
    p.add_argument("--output-dir", type=Path, required=True)
    args = p.parse_args()
    import matplotlib.pyplot as plt
    from matplotlib.colors import ListedColormap
    oracle = rows(args.oracle)
    rows(args.summary)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    # Never merge different machines, binaries, compilers, workloads, or read mixes.
    fields = ("machine", "source_id", "compiler_version", "compiler_flags",
              "experiment_family", "operations", "reads", "updates", "hot_fraction",
              "phase_length", "sample_rate", "segments")
    manifest = []
    groups = defaultdict(lambda: defaultdict(list))
    for row in oracle:
        group = tuple(row.get(k, "") for k in fields)
        groups[group][(row["zipf"], row["records"])].append(100.0 * float(row["headroom_fraction"]))
    colors = ListedColormap(["#e8eff0", "#bed9da", "#75b4b4", "#e9bf66", "#c96848"])
    for group, cells in sorted(groups.items()):
        xs = sorted({int(x[1]) for x in cells})
        ys = sorted({float(x[0]) for x in cells})
        matrix = [[float("nan") for _ in xs] for _ in ys]
        labels = [["" for _ in xs] for _ in ys]
        for (theta, records), values in cells.items():
            median = statistics.median(values)
            category = 0 if median < 1 else 1 if median < 3 else 2 if median < 5 else 3 if median < 10 else 4
            yi, xi = ys.index(float(theta)), xs.index(int(records))
            matrix[yi][xi] = category
            labels[yi][xi] = "%.1f" % median
        fig, ax = plt.subplots(figsize=(max(5, len(xs) * 1.3), max(3, len(ys) * 0.7)))
        img = ax.imshow(matrix, cmap=colors, vmin=-0.5, vmax=4.5, aspect="auto")
        ax.set_xticks(range(len(xs)), [str(x) for x in xs])
        ax.set_yticks(range(len(ys)), [str(y) for y in ys])
        for yi in range(len(ys)):
            for xi in range(len(xs)):
                if labels[yi][xi]:
                    ax.text(xi, yi, labels[yi][xi] + "%", ha="center", va="center", fontsize=7)
        ax.set(xlabel="Records", ylabel="Zipf theta",
               title="Retrospective static headroom; stratum " + slug(group))
        bar = fig.colorbar(img, ax=ax, ticks=range(5))
        bar.ax.set_yticklabels(["<1%", "1-3%", "3-5%", "5-10%", ">=10%"])
        stem = "opportunity_heatmap_" + slug(group)
        save(fig, args.output_dir, stem)
        manifest.append(dict(zip(fields, group), figure=stem))
        plt.close(fig)

    with (args.output_dir / "opportunity_figure_manifest.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=["figure"] + list(fields))
        writer.writeheader()
        writer.writerows(manifest)

    # Do not draw a win/loss map from an unpaired point estimate.
    if args.paired is None:
        return
    paired = rows(args.paired)
    by_build = defaultdict(list)
    for row in paired:
        if row.get("variant") != "ADAPT-V2" or row.get("baseline") != "STATIC":
            continue
        key = tuple(row.get(k, "") for k in ("machine", "source_id", "compiler_version",
                                             "compiler_flags", "experiment_family"))
        by_build[key].append(row)
    palette = {"WIN": "#317d69", "LOSS": "#be624d", "INDETERMINATE": "#9b9b9b",
               "INSUFFICIENT_N": "#d3d3d3"}
    for group, items in sorted(by_build.items()):
        items.sort(key=lambda r: (int(r["records"]), float(r["zipf"]), int(r["phase_length"]),
                                  int(r["sample_rate"])))
        fig, ax = plt.subplots(figsize=(9, max(3, 0.32 * len(items) + 1)))
        for i, row in enumerate(items):
            value = float(row["mean_ratio"])
            low, high = float(row["bootstrap_95_low"]), float(row["bootstrap_95_high"])
            ax.errorbar(value, i, xerr=[[max(0, value - low)], [max(0, high - value)]],
                        fmt="o", color=palette.get(row["classification"], "#9b9b9b"), capsize=2)
        labels = ["n=%s theta=%s phase=%s sample=1/%s reads=%s" %
                  (r["records"], r["zipf"], r["phase_length"], r["sample_rate"], r["reads"])
                  for r in items]
        ax.set_yticks(range(len(items)), labels, fontsize=6)
        ax.axvline(1.0, color="black", linewidth=0.8)
        ax.set(xlabel="Paired ADAPT-V2 / STATIC ratio (bootstrap 95% interval)",
               title="Measured break-even classification; stratum " + slug(group))
        save(fig, args.output_dir, "adaptation_break_even_map_" + slug(group))
        plt.close(fig)


if __name__ == "__main__":
    main()
