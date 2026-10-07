"""Fail-closed paired analysis for the small Windows scale replication."""
import argparse
import csv
import random
import statistics
from collections import defaultdict
from pathlib import Path


SCALES = (100000, 1000000, 5000000)
SEEDS = (11, 23, 37, 53, 71)
REPETITIONS = (0, 1, 2)
BOOTSTRAP_DRAWS = 10000
BOOTSTRAP_SEED = 20271007
VARIANTS = ("STATIC", "ADAPT-V2")


def integer(row, name):
    return int(row[name])


def number(row, name):
    return float(row[name])


def load_rows(raw_root):
    rows = []
    failures = list(raw_root.rglob("*.failed.json"))
    partials = list(raw_root.rglob("*.partial.csv"))
    if failures or partials:
        raise ValueError("campaign contains failure or partial markers")
    for path in sorted(raw_root.rglob("*.csv")):
        if path.name.endswith(".phases.csv"):
            continue
        with path.open(newline="", encoding="utf-8") as stream:
            parsed = list(csv.DictReader(stream))
        if len(parsed) != 1:
            raise ValueError("expected one result row: " + str(path))
        row = parsed[0]
        if row.get("experiment_family") == "adaptive_scale_replication":
            row["_origin"] = str(path)
            rows.append(row)
    if len(rows) != 90:
        raise ValueError("expected 90 primary rows, found %d" % len(rows))
    return rows


def validate_row(row):
    required = (
        "variant", "machine", "source_id", "compiler", "compiler_version",
        "compiler_flags", "git_commit", "records", "operations", "zipf",
        "reads", "updates", "hot_fraction", "phase_length", "sample_rate",
        "segments", "fanout", "candidate_fanouts", "seed", "repetition",
        "throughput_ops_sec", "rebuild_count", "rebuild_total_ms", "checksum",
        "misses", "workload_fingerprint", "workload_family", "adapt_interval",
        "warmup", "latency_sampling_rate"
    )
    missing = [name for name in required if name not in row]
    if missing:
        raise ValueError("missing columns in %s: %s" %
                         (row["_origin"], ", ".join(missing)))
    valid = (
        row["variant"] in VARIANTS and integer(row, "records") in SCALES and
        integer(row, "operations") == 1000000 and number(row, "zipf") == 0.99 and
        number(row, "reads") == 0.95 and number(row, "updates") == 0.05 and
        number(row, "hot_fraction") == 0.20 and integer(row, "phase_length") == 0 and
        integer(row, "sample_rate") == 32 and integer(row, "segments") == 8 and
        integer(row, "fanout") == 64 and integer(row, "seed") in SEEDS and
        integer(row, "repetition") in REPETITIONS and
        row["candidate_fanouts"] == "8:16:32:64:128:256" and
        row["workload_family"] == "zipf" and integer(row, "adapt_interval") == 5000 and
        integer(row, "warmup") == 10000 and
        integer(row, "latency_sampling_rate") == 128 and integer(row, "misses") == 0 and
        number(row, "throughput_ops_sec") > 0 and bool(row["git_commit"])
    )
    if not valid:
        raise ValueError("row violates frozen replication matrix: " + row["_origin"])


def pair_key(row):
    return (integer(row, "records"), integer(row, "seed"), integer(row, "repetition"))


def bootstrap_interval(pairs):
    by_seed = defaultdict(list)
    for row in pairs:
        by_seed[row["seed"]].append(row)
    if tuple(sorted(by_seed)) != SEEDS:
        raise ValueError("bootstrap requires all five seeds")
    rng = random.Random(BOOTSTRAP_SEED)
    draws = []
    for _ in range(BOOTSTRAP_DRAWS):
        sample = []
        for seed in rng.choices(list(SEEDS), k=len(SEEDS)):
            sample.extend(rng.choices(by_seed[seed], k=len(by_seed[seed])))
        draws.append(statistics.mean(row["throughput_ratio"] for row in sample))
    draws.sort()
    return draws[250], draws[9750]


def write_csv(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw-root", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    rows = load_rows(args.raw_root)
    for row in rows:
        validate_row(row)

    machines = {row["machine"] for row in rows}
    sources = {row["source_id"] for row in rows}
    commits = {row["git_commit"] for row in rows}
    toolchains = {(row["compiler"], row["compiler_version"], row["compiler_flags"])
                  for row in rows}
    if any(len(values) != 1 for values in (machines, sources, commits, toolchains)):
        raise ValueError("machine, source, commit, or toolchain changed within campaign")

    grouped = defaultdict(dict)
    for row in rows:
        key = pair_key(row)
        if row["variant"] in grouped[key]:
            raise ValueError("duplicate variant identity: " + repr(key))
        grouped[key][row["variant"]] = row
    if len(grouped) != 45:
        raise ValueError("expected 45 exact pair identities")

    pairs = []
    for (records, seed, repetition), variants in sorted(grouped.items()):
        if set(variants) != set(VARIANTS):
            raise ValueError("incomplete pair: " + repr((records, seed, repetition)))
        static, adaptive = variants["STATIC"], variants["ADAPT-V2"]
        for name in ("machine", "source_id", "compiler", "compiler_version",
                     "compiler_flags", "git_commit", "checksum", "misses",
                     "workload_fingerprint"):
            if static[name] != adaptive[name]:
                raise ValueError("pair mismatch for %s: %s" %
                                 (name, repr((records, seed, repetition))))
        static_ops = number(static, "throughput_ops_sec")
        adaptive_ops = number(adaptive, "throughput_ops_sec")
        pairs.append({
            "records": records, "seed": seed, "repetition": repetition,
            "machine": static["machine"], "source_id": static["source_id"],
            "git_commit": static["git_commit"],
            "workload_fingerprint": static["workload_fingerprint"],
            "static_ops_sec": static_ops, "adaptive_v2_ops_sec": adaptive_ops,
            "throughput_ratio": adaptive_ops / static_ops,
            "rebuild_count": integer(adaptive, "rebuild_count"),
            "rebuild_total_ms": number(adaptive, "rebuild_total_ms"),
            "static_origin": static["_origin"], "adaptive_origin": adaptive["_origin"]
        })

    summary = []
    for records in SCALES:
        cohort = [row for row in pairs if row["records"] == records]
        if len(cohort) != 15:
            raise ValueError("expected 15 exact pairs at scale %d" % records)
        low, high = bootstrap_interval(cohort)
        ratio = statistics.mean(row["throughput_ratio"] for row in cohort)
        summary.append({
            "records": records, "pairs": len(cohort),
            "static_mean_ops_sec": statistics.mean(row["static_ops_sec"] for row in cohort),
            "adaptive_v2_mean_ops_sec": statistics.mean(row["adaptive_v2_ops_sec"] for row in cohort),
            "mean_ratio": ratio,
            "median_ratio": statistics.median(row["throughput_ratio"] for row in cohort),
            "bootstrap_95_low": low, "bootstrap_95_high": high,
            "mean_rebuild_count": statistics.mean(row["rebuild_count"] for row in cohort),
            "fraction_adaptive_beats_static": statistics.mean(
                row["throughput_ratio"] > 1.0 for row in cohort),
            "classification": "SLOWER" if high < 1.0 else
                              "FASTER" if low > 1.0 else "INCONCLUSIVE"
        })

    write_csv(args.out_dir / "adaptive_scale_replication_pairs.csv", pairs)
    write_csv(args.out_dir / "adaptive_scale_replication_summary.csv", summary)
    verification = [
        "# Adaptive-scale independent replication verification", "",
        "- Primary rows: 90.", "- Exact pairs: 45 (15 per scale).",
        "- Machine fingerprint: `%s`." % next(iter(machines)),
        "- Source fingerprint: `%s`." % next(iter(sources)),
        "- Git commit: `%s`." % next(iter(commits)),
        "- Correctness: zero misses and matching checksum/workload fingerprint within every pair.",
        "- Bootstrap: hierarchical paired bootstrap, 10000 draws, RNG seed %d." % BOOTSTRAP_SEED
    ]
    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / "adaptive_scale_replication_verification.md").write_text(
        "\n".join(verification) + "\n", encoding="utf-8")
    for row in summary:
        print("records=%d ratio=%.3f CI=[%.3f, %.3f] pairs=%d" %
              (row["records"], row["mean_ratio"], row["bootstrap_95_low"],
               row["bootstrap_95_high"], row["pairs"]))


if __name__ == "__main__":
    main()
