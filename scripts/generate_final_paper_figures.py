#!/usr/bin/env python3
"""Generate figures for the final SIGMOD-style paper draft."""

import csv
import os
from collections import defaultdict

import matplotlib.pyplot as plt

plt.rcParams["pdf.fonttype"] = 42
plt.rcParams["ps.fonttype"] = 42
plt.rcParams["font.family"] = "DejaVu Sans"

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
RESULTS = os.path.join(ROOT, "results")
OUT = os.path.join(ROOT, "paper-final", "figures")
OUT_RESULTS = os.path.join(RESULTS, "figures")


def rows(path):
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle))


def f(row, key):
    try:
        return float(row[key])
    except (KeyError, TypeError, ValueError):
        return 0.0


def savefig(name):
    path = os.path.join(OUT, name)
    result_path = os.path.join(OUT_RESULTS, name)
    plt.tight_layout()
    plt.savefig(path, bbox_inches="tight")
    plt.savefig(result_path, bbox_inches="tight")
    plt.close()
    print(os.path.relpath(path, ROOT))
    print(os.path.relpath(result_path, ROOT))


def ycsb_figure():
    data = rows(os.path.join(RESULTS, "ycsb_trial_summary.csv"))
    wanted = [
        "Static B+Tree f=64",
        "Static B+Tree f=256",
        "Static Region x8",
        "Segmented Adaptive",
    ]
    colors = {
        "Static B+Tree f=64": "#4C78A8",
        "Static B+Tree f=256": "#72B7B2",
        "Static Region x8": "#F58518",
        "Segmented Adaptive": "#E45756",
    }
    workloads = ["YCSB-A", "YCSB-B", "YCSB-C"]
    width = 0.18
    x = range(len(workloads))

    plt.figure(figsize=(7.2, 3.0))
    for i, name in enumerate(wanted):
        values = []
        errors = []
        for workload in workloads:
            row = next(r for r in data if r["workload"] == workload and r["index"] == name)
            values.append(f(row, "mean_ops_sec") / 1e6)
            errors.append(f(row, "ci95_ops_sec") / 1e6)
        offsets = [p + (i - 1.5) * width for p in x]
        plt.bar(offsets, values, width=width, yerr=errors, capsize=2.5,
                label=name.replace("Static ", "").replace("Segmented ", "Seg. "),
                color=colors[name], edgecolor="black", linewidth=0.3)

    plt.xticks(list(x), workloads)
    plt.ylabel("Throughput (M ops/s)")
    plt.title("Stationary YCSB Workloads")
    plt.grid(axis="y", alpha=0.25)
    plt.legend(ncol=2, fontsize=8, frameon=False)
    savefig("fig_ycsb_throughput.pdf")


def shifting_figure():
    data = rows(os.path.join(RESULTS, "shifting_trial_summary.csv"))
    wanted = [
        "Static B+Tree f=64",
        "Oracle Hot f=32",
        "Segmented Adaptive",
    ]
    phases = ["Hot-S0", "Hot-S7", "Uniform", "Hot-S3-A"]
    markers = ["o", "s", "^"]
    colors = ["#4C78A8", "#54A24B", "#E45756"]

    plt.figure(figsize=(7.2, 3.0))
    for name, marker, color in zip(wanted, markers, colors):
        values = []
        errors = []
        for phase in phases:
            row = next(r for r in data if r["phase"] == phase and r["index"] == name)
            values.append(f(row, "mean_ops_sec") / 1e6)
            errors.append(f(row, "ci95_ops_sec") / 1e6)
        plt.errorbar(range(len(phases)), values, yerr=errors, marker=marker,
                     capsize=3, linewidth=1.8, label=name, color=color)

    plt.xticks(range(len(phases)), phases)
    plt.ylabel("Throughput (M ops/s)")
    plt.title("Shifting Hot-Region Workload by Phase")
    plt.grid(axis="y", alpha=0.25)
    plt.legend(fontsize=8, frameon=False)
    savefig("fig_shifting_phases.pdf")


def overhead_figure():
    summary_path = os.path.join(RESULTS, "overhead_trial_summary.csv")
    use_summary = os.path.exists(summary_path)
    data = rows(summary_path if use_summary else os.path.join(RESULTS, "overhead_summary.csv"))
    order = [
        "Static B+Tree",
        "Static Region",
        "Static Region + Monitor",
        "Segmented No Records",
        "Segmented Records Only",
        "Segmented Monitor + Records",
        "Segmented Full Adaptive",
    ]
    short = ["Static", "Region", "+Mon", "NoRec", "Records", "Mon+Rec", "Full"]
    workloads = ["Uniform-B", "Zipf-B", "Zipf-A"]

    plt.figure(figsize=(7.2, 3.2))
    for workload, color in zip(workloads, ["#4C78A8", "#F58518", "#E45756"]):
        base = next(r for r in data if r["workload"] == workload and r["index"] == "Static B+Tree")
        base_tput = f(base, "mean_ops_sec") if use_summary else f(base, "throughput_ops_sec")
        values = []
        for name in order:
            row = next(r for r in data if r["workload"] == workload and r["index"] == name)
            tput = f(row, "mean_ops_sec") if use_summary else f(row, "throughput_ops_sec")
            values.append(tput / base_tput)
        plt.plot(range(len(order)), values, marker="o", linewidth=1.7,
                 label=workload, color=color)

    plt.axhline(1.0, color="black", linewidth=0.8, linestyle="--")
    plt.xticks(range(len(order)), short, rotation=25, ha="right")
    plt.ylabel("Throughput / Static B+Tree")
    plt.title("Clean Overhead Ablation")
    plt.grid(axis="y", alpha=0.25)
    plt.legend(fontsize=8, frameon=False)
    savefig("fig_overhead_ablation.pdf")


def benefit_penalty_figure():
    labels = ["YCSB-A", "YCSB-B", "YCSB-C", "Shift", "Wiki"]
    # Current paper values, converted from the reported throughput summaries
    # to microseconds per operation. Stationary "benefit" is the best static
    # fanout relative to f=64; shifting uses the phase-aware oracle.
    benefit = [0.0000, 0.0091, 0.0000, 0.0167, 0.0003]
    penalty = [0.0483, 0.0561, 0.0690, 0.0169, 0.0228]
    x = range(len(labels))
    width = 0.34

    plt.figure(figsize=(7.2, 3.0))
    plt.bar([p - width / 2 for p in x], benefit, width=width,
            label="Oracle benefit", color="#54A24B", edgecolor="black", linewidth=0.3)
    plt.bar([p + width / 2 for p in x], penalty, width=width,
            label="Adaptive penalty", color="#E45756", edgecolor="black", linewidth=0.3)
    plt.xticks(list(x), labels)
    plt.ylabel("Microseconds per operation")
    plt.title("Oracle Headroom vs. Adaptive Cost")
    plt.grid(axis="y", alpha=0.25)
    plt.legend(fontsize=8, frameon=False)
    savefig("fig_benefit_vs_penalty.pdf")


def thread_scalability_figure():
    path = os.path.join(RESULTS, "thread_scalability.csv")
    if not os.path.exists(path):
        return

    data = rows(path)
    threads = [int(r["threads"]) for r in data]
    throughput = [f(r, "ops_sec") / 1e6 for r in data]
    base = throughput[0] if throughput else 1.0
    speedup = [v / base for v in throughput]

    plt.figure(figsize=(7.2, 3.15))
    ax1 = plt.gca()
    ax1.plot(threads, throughput, marker="o", markersize=5.5, linewidth=2.4,
             color="#2F5F9E", label="Throughput")
    ax1.fill_between(threads, throughput, color="#2F5F9E", alpha=0.10)
    ax1.set_xlabel("Worker threads")
    ax1.set_ylabel("Throughput (M ops/s)")
    ax1.set_xticks(threads)
    ax1.set_ylim(0, max(throughput) * 1.18)
    ax1.grid(axis="y", alpha=0.25)

    ax2 = ax1.twinx()
    ax2.plot(threads, speedup, marker="s", markersize=4.5, linewidth=1.7,
             color="#F58518", label="Speedup")
    ax2.set_ylabel("Speedup vs 1 thread")
    ax2.set_ylim(0, max(speedup) * 1.20)

    if threads and throughput:
        ax1.annotate("{:.1f}M ops/s".format(throughput[-1]),
                     xy=(threads[-1], throughput[-1]),
                     xytext=(-62, 18),
                     textcoords="offset points",
                     fontsize=8,
                     arrowprops=dict(arrowstyle="->", color="#333333", lw=0.8))
        ax1.annotate("optimistic reads\nno global latch",
                     xy=(threads[3], throughput[3]),
                     xytext=(-88, -34),
                     textcoords="offset points",
                     fontsize=8,
                     arrowprops=dict(arrowstyle="->", color="#333333", lw=0.8))

    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, fontsize=8, frameon=False,
               loc="upper left")
    plt.title("Scaling: Optimistic Lock Coupling")
    savefig("fig_thread_scalability.pdf")


def dynamic_hotspot_figure():
    path = os.path.join(RESULTS, "dynamic_hotspot_summary.csv")
    if not os.path.exists(path):
        return

    data = rows(path)
    wanted = ["Static B+Tree f=64", "Static Region x8", "Segmented Adaptive"]
    colors = {
        "Static B+Tree f=64": "#4C78A8",
        "Static Region x8": "#F58518",
        "Segmented Adaptive": "#E45756",
    }
    markers = {
        "Static B+Tree f=64": "o",
        "Static Region x8": "s",
        "Segmented Adaptive": "^",
    }

    plt.figure(figsize=(7.2, 3.15))
    first_subset = [r for r in data if r["index"] == wanted[0]]
    first_subset.sort(key=lambda r: int(r["phase"]))
    if first_subset:
        window_ops = int(float(first_subset[0].get("operations", 1)))
    else:
        window_ops = 1
    window_millions = window_ops / 1e6
    phase_count = max([int(r["phase"]) for r in data]) + 1 if data else 0

    for name in wanted:
        subset = [r for r in data if r["index"] == name]
        subset.sort(key=lambda r: int(r["phase"]))
        phases = [(int(r["phase"]) + 0.5) * window_millions for r in subset]
        values = [f(r, "ops_sec") / 1e6 for r in subset]
        plt.plot(phases, values, marker=markers[name], markersize=5,
                 linewidth=2.0 if name != "Segmented Adaptive" else 2.5,
                 label=name, color=colors[name])

    ymax = max([f(r, "ops_sec") for r in data]) / 1e6 if data else 1.0
    for boundary in range(1, phase_count):
        x = boundary * window_millions
        plt.axvline(x, color="#333333", linestyle="--", linewidth=0.8, alpha=0.55)
        if boundary == 1:
            plt.text(x + 0.03, ymax * 0.96, "hotspot shifts",
                     rotation=90, va="top", ha="left", fontsize=8,
                     color="#333333")

    adaptive = [r for r in data if r["index"] == "Segmented Adaptive"]
    if adaptive:
        adaptive.sort(key=lambda r: int(r["phase"]))
        last = adaptive[-1]
        plt.annotate("adaptive path\nfalls to 0.63x avg.",
                     xy=((int(last["phase"]) + 0.5) * window_millions,
                         f(last, "ops_sec") / 1e6),
                     xytext=(-88, 30),
                     textcoords="offset points",
                     fontsize=8,
                     arrowprops=dict(arrowstyle="->", color="#333333", lw=0.8))

    plt.xlabel("Operations elapsed (millions)")
    plt.ylabel("Throughput (M ops/s)")
    plt.xlim(0, phase_count * window_millions)
    plt.ylim(0, ymax * 1.18)
    plt.title("Shifting Hotspot: Static Stability vs Adaptive Overhead")
    plt.grid(axis="y", alpha=0.25)
    plt.legend(fontsize=8, frameon=False)
    savefig("fig_dynamic_hotspot.pdf")


def main():
    if not os.path.isdir(OUT):
        os.makedirs(OUT)
    if not os.path.isdir(OUT_RESULTS):
        os.makedirs(OUT_RESULTS)
    ycsb_figure()
    shifting_figure()
    overhead_figure()
    benefit_penalty_figure()
    thread_scalability_figure()
    dynamic_hotspot_figure()


if __name__ == "__main__":
    main()
