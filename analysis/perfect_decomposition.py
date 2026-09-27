#!/usr/bin/env python3
"""Verify and decompose the completed Perfect-V1/V2 campaign.

This script consumes existing result archives only.  It does not launch a
benchmark.  Confidence intervals use a paired hierarchical bootstrap: seeds
are sampled first, then repetitions within each sampled seed.
"""

from __future__ import print_function

import argparse
import csv
import io
import math
import os
import random
import statistics
import zipfile
from collections import defaultdict


REGIMES = {
    "zero": {"zipf": "1.2", "hot_fraction": "0.2", "phase_length": "500000"},
    "borderline": {"zipf": "0.99", "hot_fraction": "0.05", "phase_length": "500000"},
    "high": {"zipf": "0.8", "hot_fraction": "0.05", "phase_length": "10000"},
}
SEEDS = ("11", "23", "37", "53", "71")
REPETITIONS = tuple(str(i) for i in range(10))
VARIANTS = ("STATIC-REGIONAL", "PERFECT-V1", "PERFECT-V2", "ADAPT-V1", "ADAPT-V2")
FIXED = {
    "machine": "f410f7064c081e00",
    "source_id": "b9d01f8835b06ae01fb0",
    "os": "Linux",
    "compiler": "GCC",
    "compiler_version": "11.4.0",
    "compiler_flags": "-std=c++11 -O3 -march=native -DNDEBUG",
    "cpu_model": "Intel(R) Xeon(R) CPU @ 2.20GHz",
    "environment": "kaggle",
    "records": "1000000",
    "operations": "1000000",
    "reads": "0.95",
    "updates": "0.05",
    "segments": "8",
    "threads": "1",
    "candidate_fanouts": "8:16:32:64:128:256",
}


def rows_from_archive(path):
    rows = []
    phase_rows = []
    oracle_rows = []
    with zipfile.ZipFile(path) as archive:
        for name in archive.namelist():
            if not name.endswith(".csv"):
                continue
            with archive.open(name) as raw:
                reader = csv.DictReader(io.TextIOWrapper(raw, encoding="utf-8-sig", newline=""))
                current = list(reader)
            if name == "processed/dynamic_selected_phase_oracle.csv":
                for row in current:
                    row["_raw_file"] = name
                    oracle_rows.append(row)
            elif name.endswith(".csv.phases.csv"):
                for row in current:
                    row["_raw_file"] = name
                    phase_rows.append(row)
            elif name.startswith("raw/"):
                for row in current:
                    row["_raw_file"] = name
                    rows.append(row)
    return rows, phase_rows, oracle_rows


def fail(errors, message):
    errors.append(message)


def regime_of(row):
    for name, spec in REGIMES.items():
        if all(row.get(k) == v for k, v in spec.items()):
            return name
    return None


def f(row, name):
    value = row.get(name, "")
    if value in ("", "UNSUPPORTED"):
        return None
    return float(value)


def percentile(values, q):
    values = sorted(values)
    if not values:
        return float("nan")
    pos = (len(values) - 1) * q
    lo = int(math.floor(pos))
    hi = int(math.ceil(pos))
    if lo == hi:
        return values[lo]
    return values[lo] * (hi - pos) + values[hi] * (pos - lo)


def hierarchical_ci(records, metric, rng, iterations):
    by_seed = defaultdict(list)
    for record in records:
        by_seed[record["seed"]].append(record)
    seeds = sorted(by_seed)
    draws = []
    for _ in range(iterations):
        sample = []
        for _unused in seeds:
            seed = rng.choice(seeds)
            bucket = by_seed[seed]
            sample.extend(rng.choice(bucket) for _unused2 in bucket)
        draws.append(statistics.mean(metric(row) for row in sample))
    return percentile(draws, 0.025), percentile(draws, 0.975)


def oracle_ci(records, metric, rng, iterations):
    draws = []
    for _ in range(iterations):
        sample = [rng.choice(records) for _unused in records]
        draws.append(statistics.mean(metric(row) for row in sample))
    return percentile(draws, 0.025), percentile(draws, 0.975)


def expected_rebuilds(targets, segments):
    current = 64
    count = 0
    for target in targets:
        if target != current:
            count += segments
            current = target
    return count


def write_csv(path, fieldnames, rows):
    parent = os.path.dirname(path)
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", default="results-now-2.zip")
    parser.add_argument("--out-dir", default="results/processed")
    parser.add_argument("--iterations", type=int, default=10000)
    args = parser.parse_args()

    all_rows, phase_rows, oracle_rows = rows_from_archive(args.archive)
    errors = []
    relevant = []
    for row in all_rows:
        regime = regime_of(row)
        if regime is None:
            continue
        family = row.get("experiment_family", "")
        if family not in ("dynamic_selected_" + regime, "dynamic_perfect_" + regime,
                          "dynamic_selected_oracle"):
            continue
        relevant.append(row)

    selected = {}
    raw_oracle = defaultdict(list)
    exact_files = []
    for row in relevant:
        regime = regime_of(row)
        row_fixed = dict(FIXED)
        if row.get("experiment_family") != "dynamic_selected_oracle":
            row_fixed["fanout"] = "64"
        for key, expected in row_fixed.items():
            if row.get(key) != expected:
                fail(errors, "%s: %s=%r, expected %r" % (row["_raw_file"], key, row.get(key), expected))
        if row.get("seed") not in SEEDS:
            fail(errors, "%s: unexpected seed %s" % (row["_raw_file"], row.get("seed")))
        if row.get("misses") != "0":
            fail(errors, "%s: misses=%s" % (row["_raw_file"], row.get("misses")))
        if row.get("invariant_status") != "NOT_CHECKED":
            fail(errors, "%s: unexpected timed-run invariant status %s" %
                 (row["_raw_file"], row.get("invariant_status")))
        family = row["experiment_family"]
        if family == "dynamic_selected_oracle":
            if row["variant"] != "STATIC-REGIONAL" or row["repetition"] != "0":
                fail(errors, "%s: malformed raw oracle calibration row" % row["_raw_file"])
            raw_oracle[(regime, row["seed"])].append(row)
            continue
        if row.get("sample_rate") != "32":
            continue
        expected_family = "dynamic_perfect_" + regime if row["variant"].startswith("PERFECT") else "dynamic_selected_" + regime
        if family != expected_family or row["variant"] not in VARIANTS:
            continue
        key = (regime, row["seed"], row["repetition"], row["variant"])
        if key in selected:
            fail(errors, "duplicate selected key %r" % (key,))
        selected[key] = row
        exact_files.append({"regime": regime, "seed": row["seed"], "repetition": row["repetition"],
                            "variant": row["variant"], "raw_file": row["_raw_file"], "run_id": row["run_id"]})

    oracle = {}
    for row in oracle_rows:
        regime = regime_of(row)
        if regime is None:
            continue
        key = (regime, row["seed"])
        if key in oracle:
            fail(errors, "duplicate phase oracle key %r" % (key,))
        oracle[key] = row

    for regime in REGIMES:
        for seed in SEEDS:
            calibration = raw_oracle[(regime, seed)]
            if len(calibration) != 6 or {row["fanout"] for row in calibration} != {"8", "16", "32", "64", "128", "256"}:
                fail(errors, "%s/%s: expected six raw fanout calibration rows" % (regime, seed))
            if len({row["workload_fingerprint"] for row in calibration}) != 1:
                fail(errors, "%s/%s: raw oracle workload fingerprints differ" % (regime, seed))
            if len({row["checksum"] for row in calibration}) != 1:
                fail(errors, "%s/%s: raw oracle checksums differ" % (regime, seed))
            if (regime, seed) not in oracle:
                fail(errors, "%s/%s: missing processed phase oracle" % (regime, seed))
            else:
                oracle_row = oracle[(regime, seed)]
                for field in ("machine", "source_id", "compiler_version", "compiler_flags",
                              "records", "operations", "reads", "updates", "segments"):
                    expected = FIXED[field]
                    if oracle_row.get(field) != expected:
                        fail(errors, "%s/%s: processed oracle %s=%r expected %r" %
                             (regime, seed, field, oracle_row.get(field), expected))
                if oracle_row.get("sample_rate") != "32" or oracle_row.get("repetition") != "0":
                    fail(errors, "%s/%s: processed oracle must use sample_rate=32 repetition=0" % (regime, seed))
            for repetition in REPETITIONS:
                bucket = []
                for variant in VARIANTS:
                    key = (regime, seed, repetition, variant)
                    if key not in selected:
                        fail(errors, "missing selected row %r" % (key,))
                    else:
                        bucket.append(selected[key])
                if len(bucket) == len(VARIANTS):
                    if len({row["workload_fingerprint"] for row in bucket}) != 1:
                        fail(errors, "%s/%s/%s: paired workload fingerprints differ" % (regime, seed, repetition))
                    if len({row["checksum"] for row in bucket}) != 1:
                        fail(errors, "%s/%s/%s: paired checksums differ" % (regime, seed, repetition))
                    if bucket[0]["workload_fingerprint"] != calibration[0]["workload_fingerprint"]:
                        fail(errors, "%s/%s/%s: execution/oracle workload fingerprints differ" %
                             (regime, seed, repetition))
                    if bucket[0]["checksum"] != calibration[0]["checksum"]:
                        fail(errors, "%s/%s/%s: execution/oracle checksums differ" %
                             (regime, seed, repetition))

    pairs = []
    for regime in REGIMES:
        for seed in SEEDS:
            oracle_row = oracle[(regime, seed)]
            targets = [int(value) for value in oracle_row["phase_fanouts"].split(";")]
            expected = expected_rebuilds(targets, 8)
            for repetition in REPETITIONS:
                rows = {variant: selected[(regime, seed, repetition, variant)] for variant in VARIANTS}
                static = f(rows["STATIC-REGIONAL"], "throughput_ops_sec")
                record = {
                    "regime": regime, "seed": seed, "repetition": repetition,
                    "workload_fingerprint": rows["STATIC-REGIONAL"]["workload_fingerprint"],
                    "checksum": rows["STATIC-REGIONAL"]["checksum"], "misses": "0",
                    "oracle_zero_cost_ops_sec": oracle_row["oracle_zero_cost_ops_sec"],
                    "oracle_pairing": "seed/workload paired; oracle timing repetition=0",
                    "expected_perfect_rebuilds": expected,
                }
                for variant in VARIANTS:
                    prefix = variant.lower().replace("-", "_")
                    row = rows[variant]
                    throughput = f(row, "throughput_ops_sec")
                    record[prefix + "_ops_sec"] = throughput
                    record[prefix + "_vs_static"] = throughput / static
                    record[prefix + "_rebuild_count"] = row["rebuild_count"]
                    record[prefix + "_rebuild_total_ms"] = row["rebuild_total_ms"]
                    record[prefix + "_rebuild_share"] = (f(row, "rebuild_total_ms") / (1000.0 * f(row, "wall_seconds")))
                    record[prefix + "_raw_file"] = row["_raw_file"]
                for variant in ("PERFECT-V1", "PERFECT-V2"):
                    if int(rows[variant]["rebuild_count"]) != expected:
                        fail(errors, "%s/%s/%s/%s: rebuild_count=%s expected=%s" %
                             (regime, seed, repetition, variant, rows[variant]["rebuild_count"], expected))
                oracle_ops = float(oracle_row["oracle_zero_cost_ops_sec"])
                for version in ("v1", "v2"):
                    perfect = record["perfect_" + version + "_ops_sec"]
                    adaptive = record["adapt_" + version + "_ops_sec"]
                    record["actuation_loss_" + version + "_ops_sec"] = oracle_ops - perfect
                    record["actuation_penalty_" + version + "_us_per_op"] = 1e6 / perfect - 1e6 / oracle_ops
                    record["online_control_loss_" + version + "_ops_sec"] = perfect - adaptive
                    record["online_control_penalty_" + version + "_us_per_op"] = 1e6 / adaptive - 1e6 / perfect
                pairs.append(record)

    if errors:
        raise SystemExit("Perfect verification failed (%d errors):\n%s" % (len(errors), "\n".join(errors[:100])))

    rng = random.Random(20260928)
    summary = []
    for regime in REGIMES:
        group = [row for row in pairs if row["regime"] == regime]
        oracle_group = [oracle[(regime, seed)] for seed in SEEDS]
        for variant in ("STATIC-REGIONAL", "ORACLE-ZERO-COST", "PERFECT-V1", "PERFECT-V2", "ADAPT-V1", "ADAPT-V2"):
            if variant == "ORACLE-ZERO-COST":
                values = [float(row["oracle_zero_cost_ops_sec"]) for row in oracle_group]
                lo, hi = oracle_ci(oracle_group, lambda row: float(row["oracle_zero_cost_ops_sec"]), rng, args.iterations)
                ratio_values = [float(oracle[(regime, row["seed"])]["oracle_zero_cost_ops_sec"]) /
                                row["static_regional_ops_sec"] for row in group]
                ratio_lo, ratio_hi = hierarchical_ci(
                    group, lambda row: float(oracle[(regime, row["seed"])]["oracle_zero_cost_ops_sec"]) /
                    row["static_regional_ops_sec"], rng, args.iterations)
                n = 5
                pairing = "seed paired; measured oracle repetition=0"
            else:
                prefix = variant.lower().replace("-", "_")
                values = [row[prefix + "_ops_sec"] for row in group]
                lo, hi = hierarchical_ci(group, lambda row, p=prefix: row[p + "_ops_sec"], rng, args.iterations)
                ratio_values = [row[prefix + "_vs_static"] for row in group]
                ratio_lo, ratio_hi = hierarchical_ci(group, lambda row, p=prefix: row[p + "_vs_static"], rng, args.iterations)
                n = 50
                pairing = "seed+repetition+fingerprint paired"
            item = {
                "regime": regime, "variant": variant, "n": n,
                "mean_ops_sec": statistics.mean(values), "median_ops_sec": statistics.median(values),
                "ci95_low_ops_sec": lo, "ci95_high_ops_sec": hi,
                "mean_vs_static": statistics.mean(ratio_values), "median_vs_static": statistics.median(ratio_values),
                "ci95_low_vs_static": ratio_lo, "ci95_high_vs_static": ratio_hi,
                "pairing": pairing,
            }
            if variant in ("PERFECT-V1", "PERFECT-V2", "ADAPT-V1", "ADAPT-V2"):
                prefix = variant.lower().replace("-", "_")
                item["mean_rebuild_count"] = statistics.mean(float(row[prefix + "_rebuild_count"]) for row in group)
                item["mean_rebuild_total_ms"] = statistics.mean(float(row[prefix + "_rebuild_total_ms"]) for row in group)
                item["mean_rebuild_share"] = statistics.mean(float(row[prefix + "_rebuild_share"]) for row in group)
            summary.append(item)

        for version in ("v1", "v2"):
            for kind, field, time_field in (
                    ("ACTUATION-LOSS-" + version.upper(), "actuation_loss_" + version + "_ops_sec",
                     "actuation_penalty_" + version + "_us_per_op"),
                    ("ONLINE-CONTROL-LOSS-" + version.upper(), "online_control_loss_" + version + "_ops_sec",
                     "online_control_penalty_" + version + "_us_per_op")):
                values = [row[field] for row in group]
                lo, hi = hierarchical_ci(group, lambda row, key=field: row[key], rng, args.iterations)
                time_values = [row[time_field] for row in group]
                time_lo, time_hi = hierarchical_ci(group, lambda row, key=time_field: row[key], rng, args.iterations)
                summary.append({"regime": regime, "variant": kind, "n": 50,
                                "mean_ops_sec": statistics.mean(values), "median_ops_sec": statistics.median(values),
                                "ci95_low_ops_sec": lo, "ci95_high_ops_sec": hi,
                                "mean_us_per_op": statistics.mean(time_values),
                                "median_us_per_op": statistics.median(time_values),
                                "ci95_low_us_per_op": time_lo, "ci95_high_us_per_op": time_hi,
                                "pairing": "paired difference; positive means loss"})

    memory = []
    memory_fields = ("peak_rss_bytes", "tree_bytes", "bytes_per_key", "shadow_bytes", "monitor_bytes", "temp_rebuild_bytes")
    for regime in REGIMES:
        for variant in VARIANTS:
            rows = [selected[(regime, seed, repetition, variant)] for seed in SEEDS for repetition in REPETITIONS]
            item = {"regime": regime, "variant": variant, "n": len(rows)}
            for field in memory_fields:
                # V1 writes zero for temp_rebuild_bytes because that component
                # is not instrumented; do not turn that sentinel into evidence.
                if field == "temp_rebuild_bytes" and variant in ("ADAPT-V1", "PERFECT-V1"):
                    values = []
                else:
                    values = [f(row, field) for row in rows]
                values = [value for value in values if value is not None]
                item[field + "_supported_n"] = len(values)
                item[field + "_mean"] = statistics.mean(values) if values else ""
                item[field + "_median"] = statistics.median(values) if values else ""
            memory.append(item)

    os.makedirs(args.out_dir, exist_ok=True)
    pair_path = os.path.join(args.out_dir, "perfect_decomposition_pairs.csv")
    summary_path = os.path.join(args.out_dir, "perfect_decomposition_summary.csv")
    memory_path = os.path.join(args.out_dir, "perfect_memory_summary.csv")
    files_path = os.path.join(args.out_dir, "perfect_raw_file_inventory.csv")
    write_csv(pair_path, list(pairs[0]), pairs)
    fields = sorted({key for row in summary for key in row})
    write_csv(summary_path, fields, summary)
    write_csv(memory_path, list(memory[0]), memory)
    write_csv(files_path, ("regime", "seed", "repetition", "variant", "raw_file", "run_id"), exact_files)

    table_dir = os.path.join("sigmod_submission", "tables")
    os.makedirs(table_dir, exist_ok=True)
    table_path = os.path.join(table_dir, "perfect_decomposition.tex")
    with open(table_path, "w", encoding="ascii") as table:
        table.write("\\begin{tabular}{llrrr}\n\\toprule\n")
        table.write("Regime & Variant & Mean vs. static & 95\\% CI & Rebuild share \\\\\n\\midrule\n")
        for regime in ("zero", "borderline", "high"):
            rows = [row for row in summary if row["regime"] == regime and row["variant"] in
                    ("ORACLE-ZERO-COST", "PERFECT-V1", "PERFECT-V2", "ADAPT-V1", "ADAPT-V2")]
            for row in rows:
                share = "--" if "mean_rebuild_share" not in row else "%.1f\\%%" % (100.0 * row["mean_rebuild_share"])
                label = row["variant"].replace("ORACLE-ZERO-COST", "Oracle zero-cost")
                table.write("%s & %s & %.3f & [%.3f, %.3f] & %s \\\\\n" %
                            (regime.capitalize(), label, row["mean_vs_static"],
                             row["ci95_low_vs_static"], row["ci95_high_vs_static"], share))
            table.write("\\midrule\n" if regime != "high" else "")
        table.write("\\bottomrule\n\\end{tabular}\n")

    memory_table_path = os.path.join(table_dir, "perfect_memory.tex")
    with open(memory_table_path, "w", encoding="ascii") as table:
        table.write("\\begin{tabular}{llrrr}\n\\toprule\n")
        table.write("Regime & Variant & RSS (MiB) & Tree B/key & Monitor (KiB) \\\\\n\\midrule\n")
        for row in memory:
            if row["variant"] not in ("STATIC-REGIONAL", "PERFECT-V1", "PERFECT-V2", "ADAPT-V1", "ADAPT-V2"):
                continue
            rss = float(row["peak_rss_bytes_mean"]) / (1024.0 * 1024.0)
            bpk = row["bytes_per_key_mean"] if row["bytes_per_key_mean"] != "" else None
            monitor = row["monitor_bytes_mean"] if row["monitor_bytes_mean"] != "" else None
            table.write("%s & %s & %.1f & %s & %s \\\\\n" %
                        (row["regime"].capitalize(), row["variant"], rss,
                         ("%.2f" % float(bpk)) if bpk is not None else "--",
                         ("%.1f" % (float(monitor) / 1024.0)) if monitor is not None else "--"))
        table.write("\\bottomrule\n\\end{tabular}\n")

    report_path = os.path.join(args.out_dir, "perfect_verification.md")
    with open(report_path, "w", encoding="utf-8") as report:
        report.write("# Perfect Campaign Verification\n\n")
        report.write("VERIFIED against `%s` without running benchmarks.\n\n" % args.archive)
        report.write("- 300 Perfect rows: 3 regimes x 5 seeds x 10 repetitions x 2 variants.\n")
        report.write("- 450 matched selected rows at sample rate 32: STATIC-REGIONAL, ADAPT-V1, ADAPT-V2.\n")
        report.write("- 90 raw oracle calibration rows and 15 processed per-seed phase oracles.\n")
        report.write("- Every five-way execution pair matches seed, repetition, workload fingerprint, checksum, and zero misses; fingerprints/checksums also match the corresponding raw oracle calibration stream.\n")
        report.write("- Every Perfect rebuild count equals fanout transitions in its measured phase-oracle schedule times eight segments.\n")
        report.write("- Timed rows report `NOT_CHECKED`; invariant evidence is separate and is not inferred here.\n")
        report.write("- Oracle timing is measured only at repetition 0 per seed. Reusing its deterministic schedule for repetitions 1-9 is workload pairing, not an independent timing replicate.\n\n")
        report.write("## Extreme high-opportunity regime\n\n")
        high = [row for row in summary if row["regime"] == "high" and row["variant"] in
                ("STATIC-REGIONAL", "ORACLE-ZERO-COST", "PERFECT-V1", "PERFECT-V2", "ADAPT-V1", "ADAPT-V2")]
        report.write("| Variant | Mean ops/s | Median ops/s | Mean vs static | 95% CI ratio | Mean rebuilds | Rebuild routine share |\n")
        report.write("|---|---:|---:|---:|---:|---:|---:|\n")
        for row in high:
            report.write("| %s | %.0f | %.0f | %.4f | [%.4f, %.4f] | %s | %s |\n" %
                         (row["variant"], row["mean_ops_sec"], row["median_ops_sec"], row.get("mean_vs_static", float("nan")),
                          row.get("ci95_low_vs_static", float("nan")), row.get("ci95_high_vs_static", float("nan")),
                          ("%.1f" % row["mean_rebuild_count"]) if "mean_rebuild_count" in row else "--",
                          ("%.1f%%" % (100.0 * row["mean_rebuild_share"])) if "mean_rebuild_share" in row else "--"))
        report.write("\nThe phase length is 10,000 operations (100 phases). Perfect applies each measured target to all eight segments and charges those rebuilds inside `wall_seconds`. The extreme ratio is therefore an observed actuation cost, not a predictor error. `rebuild_total_ms/wall_seconds` reports only timed rebuild routines and is a lower bound on full actuation/control overhead.\n")

    print("Perfect verification passed")
    print("  paired execution rows:", len(pairs))
    print("  exact raw inventory:", files_path)
    print("  decomposition:", summary_path)
    print("  memory:", memory_path)
    print("  report:", report_path)
    print("  paper tables:", table_path, memory_table_path)


if __name__ == "__main__":
    main()
