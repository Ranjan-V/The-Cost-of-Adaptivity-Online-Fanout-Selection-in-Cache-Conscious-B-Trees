"""Validate and decompose the frozen V2 phase-length campaign."""
import argparse
import csv
import random
import statistics
from collections import defaultdict
from pathlib import Path


LENGTHS = (10000, 100000, 1000000, 5000000, 10000000)
SEEDS = (11, 23, 37, 53, 71)
REPETITIONS = tuple(range(5))
VARIANTS = ("STATIC-REGIONAL", "PERFECT-V2", "ADAPT-V2")
BOOTSTRAP_DRAWS = 10000
BOOTSTRAP_SEED = 20271008


def read_primary(root, family):
    rows = []
    relevant_dirs = [p for p in root.rglob(family) if p.is_dir()]
    if len(relevant_dirs) != 1:
        raise ValueError("expected one %s result directory, found %d" %
                         (family, len(relevant_dirs)))
    campaign = relevant_dirs[0]
    if list(campaign.glob("*.partial.csv")) or list(campaign.glob("*.failed.json")):
        raise ValueError(family + " contains partial or failure markers")
    for path in sorted(campaign.glob("*.csv")):
        if path.name.endswith(".phases.csv"):
            continue
        with path.open(newline="", encoding="utf-8") as stream:
            parsed = list(csv.DictReader(stream))
        if len(parsed) != 1:
            raise ValueError("expected one row: " + str(path))
        row = parsed[0]
        if row.get("experiment_family") != family:
            raise ValueError("family mismatch: " + str(path))
        row["_origin"] = str(path)
        rows.append(row)
    return campaign, rows


def integer(row, name):
    return int(row[name])


def number(row, name):
    return float(row[name])


def validate_common(row):
    valid = (
        integer(row, "records") == 1000000 and integer(row, "operations") == 10000000 and
        integer(row, "phase_length") in LENGTHS and integer(row, "seed") in SEEDS and
        number(row, "zipf") == 0.99 and number(row, "reads") == 0.95 and
        number(row, "updates") == 0.05 and number(row, "hot_fraction") == 0.05 and
        integer(row, "sample_rate") == 32 and integer(row, "segments") == 8 and
        row["candidate_fanouts"] == "8:16:32:64:128:256" and
        integer(row, "misses") == 0 and row.get("workload_family") == "shifting" and
        integer(row, "adapt_interval") == 5000 and integer(row, "warmup") == 10000 and
        integer(row, "latency_sampling_rate") == 128 and bool(row.get("git_commit"))
    )
    if not valid:
        raise ValueError("row violates frozen matrix: " + row["_origin"])


def one_provenance(rows, label):
    values = {(r["machine"], r["source_id"], r["compiler"],
               r["compiler_version"], r["compiler_flags"], r["git_commit"])
              for r in rows}
    if len(values) != 1:
        raise ValueError(label + " contains mixed provenance")
    return next(iter(values))


def load_oracle(path):
    with path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) != 25:
        raise ValueError("expected 25 measured oracle rows, found %d" % len(rows))
    indexed = {}
    for row in rows:
        key = (integer(row, "phase_length"), integer(row, "seed"))
        if key in indexed or key[0] not in LENGTHS or key[1] not in SEEDS:
            raise ValueError("invalid/duplicate oracle identity: " + repr(key))
        if row["variant"] != "ORACLE-ZERO-COST":
            raise ValueError("oracle file contains non-oracle row")
        indexed[key] = row
    return indexed


def hierarchical_ci(rows, metric):
    by_seed = defaultdict(list)
    for row in rows:
        by_seed[row["seed"]].append(row)
    if tuple(sorted(by_seed)) != SEEDS:
        raise ValueError("bootstrap requires all five seeds")
    rng = random.Random(BOOTSTRAP_SEED)
    draws = []
    for _ in range(BOOTSTRAP_DRAWS):
        sample = []
        for seed in rng.choices(list(SEEDS), k=len(SEEDS)):
            sample.extend(rng.choices(by_seed[seed], k=len(by_seed[seed])))
        draws.append(statistics.mean(metric(row) for row in sample))
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
    parser.add_argument("--oracle-csv", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()

    oracle_dir, oracle_grid = read_primary(args.raw_root, "phase_length_v2_oracle_grid")
    main_dir, main_rows = read_primary(args.raw_root, "phase_length_v2")
    if len(oracle_grid) != 150 or len(main_rows) != 375:
        raise ValueError("expected 150 oracle-grid and 375 main rows")
    for row in oracle_grid + main_rows:
        validate_common(row)
    provenance = one_provenance(oracle_grid, "oracle grid")
    if one_provenance(main_rows, "main campaign") != provenance:
        raise ValueError("oracle and main campaign provenance differ")
    if len(list(oracle_dir.glob("*.phases.csv"))) != 150 or len(list(main_dir.glob("*.phases.csv"))) != 375:
        raise ValueError("phase sidecar count mismatch")

    fanout_groups = defaultdict(dict)
    for row in oracle_grid:
        if row["variant"] != "STATIC-REGIONAL" or integer(row, "repetition") != 0:
            raise ValueError("invalid oracle-grid variant/repetition")
        key = (integer(row, "phase_length"), integer(row, "seed"))
        fanout = integer(row, "fanout")
        if fanout in fanout_groups[key]:
            raise ValueError("duplicate oracle-grid fanout")
        fanout_groups[key][fanout] = row
    expected_fanouts = {8, 16, 32, 64, 128, 256}
    if len(fanout_groups) != 25 or any(set(group) != expected_fanouts
                                      for group in fanout_groups.values()):
        raise ValueError("oracle grid is not six fanouts x 25 seed/length cohorts")

    oracle = load_oracle(args.oracle_csv)
    trios = defaultdict(dict)
    for row in main_rows:
        if (row["variant"] not in VARIANTS or integer(row, "fanout") != 64 or
                integer(row, "repetition") not in REPETITIONS):
            raise ValueError("invalid main campaign row: " + row["_origin"])
        key = (integer(row, "phase_length"), integer(row, "seed"),
               integer(row, "repetition"))
        if row["variant"] in trios[key]:
            raise ValueError("duplicate trio member: " + repr(key))
        trios[key][row["variant"]] = row
    if len(trios) != 125:
        raise ValueError("expected 125 exact trio identities")

    paired = []
    for (length, seed, repetition), variants in sorted(trios.items()):
        if set(variants) != set(VARIANTS):
            raise ValueError("incomplete trio")
        static, perfect, adaptive = (variants[name] for name in VARIANTS)
        for name in ("machine", "source_id", "compiler_version", "compiler_flags",
                     "git_commit", "checksum", "misses", "workload_fingerprint"):
            if len({static[name], perfect[name], adaptive[name]}) != 1:
                raise ValueError("trio mismatch for %s" % name)
        oracle_row = oracle[(length, seed)]
        for name in ("machine", "source_id", "compiler_version", "compiler_flags"):
            if oracle_row[name] != static[name]:
                raise ValueError("oracle/main mismatch for %s" % name)
        static_ops = number(static, "throughput_ops_sec")
        perfect_ops = number(perfect, "throughput_ops_sec")
        adaptive_ops = number(adaptive, "throughput_ops_sec")
        oracle_ops = number(oracle_row, "oracle_zero_cost_ops_sec")
        paired.append({
            "phase_length": length, "seed": seed, "repetition": repetition,
            "static_ops_sec": static_ops, "oracle_ops_sec": oracle_ops,
            "perfect_v2_ops_sec": perfect_ops, "adaptive_v2_ops_sec": adaptive_ops,
            "oracle_ratio": oracle_ops / static_ops,
            "perfect_ratio": perfect_ops / static_ops,
            "adaptive_ratio": adaptive_ops / static_ops,
            "actuation_loss": oracle_ops / static_ops - perfect_ops / static_ops,
            "online_control_loss": perfect_ops / static_ops - adaptive_ops / static_ops,
            "perfect_rebuild_count": integer(perfect, "rebuild_count"),
            "perfect_rebuild_ms": number(perfect, "rebuild_total_ms"),
            "adaptive_rebuild_count": integer(adaptive, "rebuild_count"),
            "adaptive_rebuild_ms": number(adaptive, "rebuild_total_ms")
        })

    summary = []
    for length in LENGTHS:
        cohort = [row for row in paired if row["phase_length"] == length]
        result = {"phase_length": length, "trios": len(cohort)}
        for name in ("oracle_ratio", "perfect_ratio", "adaptive_ratio",
                     "actuation_loss", "online_control_loss"):
            result[name + "_mean"] = statistics.mean(row[name] for row in cohort)
            low, high = hierarchical_ci(cohort, lambda row, n=name: row[n])
            result[name + "_ci_low"] = low
            result[name + "_ci_high"] = high
        result["perfect_rebuild_count_mean"] = statistics.mean(
            row["perfect_rebuild_count"] for row in cohort)
        result["perfect_rebuild_ms_mean"] = statistics.mean(
            row["perfect_rebuild_ms"] for row in cohort)
        result["adaptive_rebuild_count_mean"] = statistics.mean(
            row["adaptive_rebuild_count"] for row in cohort)
        result["adaptive_rebuild_ms_mean"] = statistics.mean(
            row["adaptive_rebuild_ms"] for row in cohort)
        summary.append(result)

    write_csv(args.out_dir / "phase_length_v2_pairs.csv", paired)
    write_csv(args.out_dir / "phase_length_v2_summary.csv", summary)
    verification = [
        "# Phase-length V2 verification", "",
        "- Oracle-grid rows / sidecars: 150 / 150.",
        "- Main rows / sidecars: 375 / 375.",
        "- Exact trios: 125 (25 per phase length).",
        "- Measured oracle schedules: 25 (five seeds per phase length).",
        "- Machine: `%s`." % provenance[0], "- Source: `%s`." % provenance[1],
        "- Correctness: zero misses and matching checksum/workload fingerprint within each trio.",
        "- Bootstrap: 10000 hierarchical paired draws, seed %d." % BOOTSTRAP_SEED
    ]
    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / "phase_length_v2_verification.md").write_text(
        "\n".join(verification) + "\n", encoding="utf-8")
    for row in summary:
        print("phase=%d oracle=%.3f perfect=%.3f adaptive=%.3f" %
              (row["phase_length"], row["oracle_ratio_mean"],
               row["perfect_ratio_mean"], row["adaptive_ratio_mean"]))


if __name__ == "__main__":
    main()
