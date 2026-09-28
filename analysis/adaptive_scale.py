"""Validate and summarize the frozen Adaptive-V2 scale campaign.

New STATIC/ADAPT-V2 rows are paired exactly.  The completed static fanout
sweep is analyzed as a separate historical oracle cohort; it is never treated
as an exact pair for a newly compiled adaptive binary.
"""
import argparse
import csv
import io
import random
import statistics
import zipfile
from collections import defaultdict
from pathlib import Path

SCALES = (100000, 1000000, 5000000)
SEEDS = (11, 23, 37, 53, 71)
REPETITIONS = tuple(range(10))
FANOUTS = (8, 16, 32, 64, 128, 256)
BOOTSTRAP_SEED = 20270928
BOOTSTRAP_DRAWS = 10000

REQUIRED = {
    "run_id", "experiment_family", "timestamp", "source_id", "variant",
    "compiler", "compiler_version", "compiler_flags", "machine", "seed",
    "repetition", "records", "operations", "zipf", "reads", "updates",
    "hot_fraction", "phase_length", "sample_rate", "segments", "fanout",
    "candidate_fanouts", "wall_seconds", "throughput_ops_sec",
    "monitor_work_ms", "rebuild_count", "rebuild_total_ms", "checksum",
    "misses", "workload_fingerprint"
}


def completed_csv_rows(path):
    """Yield (origin, row) from a directory or ZIP without extracting it."""
    path = Path(path)
    if path.is_dir():
        for file in sorted(path.rglob("*.csv")):
            if file.name.endswith(".phases.csv") or ".partial" in file.name:
                continue
            normalized = str(file).replace("\\", "/")
            if "/raw/" not in normalized and path.name != "raw":
                continue
            with file.open(newline="", encoding="utf-8") as stream:
                rows = list(csv.DictReader(stream))
            if len(rows) != 1:
                raise ValueError("expected one row in " + str(file))
            yield str(file), rows[0]
    elif path.is_file() and path.suffix.lower() == ".zip":
        with zipfile.ZipFile(str(path)) as archive:
            for name in sorted(archive.namelist()):
                if not name.endswith(".csv") or name.endswith(".phases.csv") or ".partial" in name:
                    continue
                if "/raw/" not in ("/" + name.replace("\\", "/")):
                    continue
                text = archive.read(name).decode("utf-8-sig")
                rows = list(csv.DictReader(io.StringIO(text)))
                if len(rows) != 1:
                    raise ValueError("expected one row in %s:%s" % (path, name))
                yield "%s:%s" % (path, name), rows[0]
    else:
        raise ValueError("input must be a result directory or ZIP: " + str(path))


def require_columns(row, origin):
    missing = sorted(REQUIRED.difference(row))
    if missing:
        raise ValueError("missing columns in %s: %s" % (origin, ", ".join(missing)))


def f(row, name):
    return float(row[name])


def i(row, name):
    return int(row[name])


def pair_key(row):
    return tuple(row[k] for k in (
        "machine", "source_id", "compiler", "compiler_version", "compiler_flags",
        "experiment_family", "records", "operations", "zipf", "reads", "updates",
        "hot_fraction", "phase_length", "sample_rate", "segments", "fanout",
        "candidate_fanouts", "seed", "repetition", "workload_fingerprint", "git_commit"))


def validate_new(row, origin):
    require_columns(row, origin)
    if row["experiment_family"] != "adaptive_scale":
        return False
    expected = (
        row["variant"] in ("STATIC", "ADAPT-V2") and
        i(row, "records") in SCALES and i(row, "operations") == 1000000 and
        f(row, "zipf") == 0.99 and f(row, "reads") == 0.95 and
        f(row, "updates") == 0.05 and f(row, "hot_fraction") == 0.20 and
        i(row, "phase_length") == 0 and i(row, "sample_rate") == 32 and
        i(row, "segments") == 8 and i(row, "fanout") == 64 and
        i(row, "seed") in SEEDS and i(row, "repetition") in REPETITIONS and
        row["candidate_fanouts"] == "8:16:32:64:128:256" and
        row.get("workload_family") == "zipf" and
        int(row.get("adapt_interval", -1)) == 5000 and
        int(row.get("warmup", -1)) == 10000 and
        int(row.get("latency_sampling_rate", -1)) == 128 and
        bool(row.get("git_commit")))
    if not expected:
        raise ValueError("row violates frozen adaptive-scale matrix: " + origin)
    if i(row, "misses") != 0:
        raise ValueError("nonzero misses: " + origin)
    if not row["checksum"] or not row["workload_fingerprint"]:
        raise ValueError("missing correctness identity: " + origin)
    return True


def hierarchical_bootstrap(pairs, metric):
    by_seed = defaultdict(list)
    for row in pairs:
        by_seed[row["seed"]].append(row)
    if tuple(sorted(by_seed)) != SEEDS:
        raise ValueError("hierarchical bootstrap requires all five frozen seeds")
    rng = random.Random(BOOTSTRAP_SEED)
    seed_values = list(SEEDS)
    draws = []
    for _ in range(BOOTSTRAP_DRAWS):
        sample = []
        for seed in rng.choices(seed_values, k=len(seed_values)):
            repetitions = by_seed[seed]
            sample.extend(rng.choices(repetitions, k=len(repetitions)))
        draws.append(statistics.mean(metric(row) for row in sample))
    draws.sort()
    return draws[250], draws[9750]


def write_csv(path, rows, fields):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def load_new(path):
    groups = defaultdict(dict)
    origins = {}
    for origin, row in completed_csv_rows(path):
        if not validate_new(row, origin):
            continue
        key = pair_key(row)
        variant = row["variant"]
        if variant in groups[key]:
            raise ValueError("duplicate completed experiment identity: " + origin)
        groups[key][variant] = row
        origins[(key, variant)] = origin
    expected = len(SCALES) * len(SEEDS) * len(REPETITIONS)
    if len(groups) != expected:
        raise ValueError("expected %d paired identities, found %d" % (expected, len(groups)))
    pairs = []
    for key, variants in groups.items():
        if set(variants) != {"STATIC", "ADAPT-V2"}:
            raise ValueError("incomplete variant pair: " + repr(key))
        static, adaptive = variants["STATIC"], variants["ADAPT-V2"]
        for name in ("workload_fingerprint", "checksum", "misses", "machine",
                     "source_id", "compiler", "compiler_version", "compiler_flags", "git_commit"):
            if static[name] != adaptive[name]:
                raise ValueError("pair mismatch for %s: %s" % (name, repr(key)))
        ts, ta = f(static, "throughput_ops_sec"), f(adaptive, "throughput_ops_sec")
        if ts <= 0 or ta <= 0:
            raise ValueError("nonpositive throughput")
        operations = i(adaptive, "operations")
        rebuild_us_op = f(adaptive, "rebuild_total_ms") * 1000.0 / operations
        penalty_us_op = 1000000.0 / ta - 1000000.0 / ts
        pairs.append({
            "records": i(static, "records"), "seed": i(static, "seed"),
            "repetition": i(static, "repetition"), "machine": static["machine"],
        "source_id": static["source_id"], "git_commit": static["git_commit"],
            "workload_fingerprint": static["workload_fingerprint"],
            "static_ops_sec": ts, "adaptive_v2_ops_sec": ta, "throughput_ratio": ta / ts,
            "adaptive_penalty_us_op": penalty_us_op,
            "rebuild_count": i(adaptive, "rebuild_count"),
            "rebuild_total_ms": f(adaptive, "rebuild_total_ms"),
            "rebuild_time_fraction": f(adaptive, "rebuild_total_ms") / (f(adaptive, "wall_seconds") * 1000.0),
            "rebuild_us_op": rebuild_us_op,
            "derived_recurring_penalty_us_op": penalty_us_op - rebuild_us_op,
            "monitor_work_ms": adaptive["monitor_work_ms"],
            "static_origin": origins[(key, "STATIC")], "adaptive_origin": origins[(key, "ADAPT-V2")]
        })
    return pairs


def load_historical_oracle(path):
    groups = defaultdict(dict)
    provenance = defaultdict(set)
    for origin, row in completed_csv_rows(path):
        require_columns(row, origin)
        if row["experiment_family"] != "static_opportunity" or row["variant"] != "STATIC":
            continue
        if (i(row, "records") not in SCALES or i(row, "operations") != 1000000 or
                f(row, "zipf") != 0.99 or f(row, "reads") != 0.95 or
                f(row, "updates") != 0.05 or f(row, "hot_fraction") != 0.20 or
                i(row, "phase_length") != 0 or i(row, "sample_rate") != 32 or
                i(row, "segments") != 8 or row["candidate_fanouts"] != "8:16:32:64:128:256" or
                i(row, "seed") not in SEEDS or i(row, "repetition") != 0 or
                i(row, "fanout") not in FANOUTS):
            continue
        if i(row, "misses") != 0:
            raise ValueError("historical oracle row has misses: " + origin)
        key = (i(row, "records"), i(row, "seed"), row["machine"], row["source_id"],
               row["compiler_version"], row["compiler_flags"], row["workload_fingerprint"])
        fanout = i(row, "fanout")
        if fanout in groups[key]:
            raise ValueError("duplicate historical fanout row: " + origin)
        groups[key][fanout] = row
        provenance[i(row, "records")].add((row["machine"], row["source_id"], row["compiler_version"], row["compiler_flags"]))
    rows = []
    expected = len(SCALES) * len(SEEDS)
    if len(groups) != expected:
        raise ValueError("expected %d historical oracle seed cohorts, found %d" % (expected, len(groups)))
    for key, fanouts in groups.items():
        if set(fanouts) != set(FANOUTS):
            raise ValueError("historical oracle cohort lacks the frozen six fanouts")
        baseline = fanouts[64]
        best_fanout, best = max(fanouts.items(), key=lambda item: f(item[1], "throughput_ops_sec"))
        tb, to = f(baseline, "throughput_ops_sec"), f(best, "throughput_ops_sec")
        rows.append({"records": key[0], "seed": key[1], "best_fanout": best_fanout,
                     "oracle_ratio": to / tb, "oracle_headroom_pct": 100.0 * (to / tb - 1.0),
                     "oracle_saving_us_op": 1000000.0 / tb - 1000000.0 / to,
                     "machine": key[2], "source_id": key[3], "reuse_status": "HISTORICAL_UNPAIRED"})
    return rows, provenance


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--new-raw", type=Path, required=True)
    parser.add_argument("--static-archive", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, default=Path("results/processed"))
    args = parser.parse_args()
    pairs = load_new(args.new_raw)
    oracle, provenance = load_historical_oracle(args.static_archive)
    pair_fields = list(pairs[0])
    write_csv(args.out_dir / "adaptive_scale_pairs.csv", pairs, pair_fields)
    write_csv(args.out_dir / "adaptive_scale_oracle.csv", oracle, list(oracle[0]))

    per_seed = []
    seed_cohorts = defaultdict(list)
    for row in pairs:
        seed_cohorts[(row["records"], row["seed"])].append(row)
    for (records, seed), cohort in sorted(seed_cohorts.items()):
        per_seed.append({"records": records, "seed": seed, "pairs": len(cohort),
                         "mean_ratio": statistics.mean(r["throughput_ratio"] for r in cohort),
                         "median_ratio": statistics.median(r["throughput_ratio"] for r in cohort),
                         "mean_penalty_us_op": statistics.mean(r["adaptive_penalty_us_op"] for r in cohort)})
    write_csv(args.out_dir / "adaptive_scale_per_seed.csv", per_seed, list(per_seed[0]))

    summary = []
    for records in SCALES:
        cohort = [r for r in pairs if r["records"] == records]
        if len(cohort) != len(SEEDS) * len(REPETITIONS):
            raise ValueError("incomplete scale cohort: " + str(records))
        low, high = hierarchical_bootstrap(cohort, lambda r: r["throughput_ratio"])
        historical = [r for r in oracle if r["records"] == records]
        ratio = statistics.mean(r["throughput_ratio"] for r in cohort)
        classification = "SLOWER" if high < 1.0 else "MAY_PAY_AUDIT_REQUIRED" if low > 1.0 else "INCONCLUSIVE"
        summary.append({
            "records": records, "pairs": len(cohort),
            "static_mean_ops_sec": statistics.mean(r["static_ops_sec"] for r in cohort),
            "adaptive_v2_mean_ops_sec": statistics.mean(r["adaptive_v2_ops_sec"] for r in cohort),
            "mean_ratio": ratio, "median_ratio": statistics.median(r["throughput_ratio"] for r in cohort),
            "bootstrap_95_low": low, "bootstrap_95_high": high,
            "mean_penalty_us_op": statistics.mean(r["adaptive_penalty_us_op"] for r in cohort),
            "mean_rebuild_count": statistics.mean(r["rebuild_count"] for r in cohort),
            "mean_rebuild_time_fraction": statistics.mean(r["rebuild_time_fraction"] for r in cohort),
            "mean_recurring_penalty_us_op": statistics.mean(r["derived_recurring_penalty_us_op"] for r in cohort),
            "fraction_adaptive_beats_static": sum(r["throughput_ratio"] > 1.0 for r in cohort) / float(len(cohort)),
            "historical_oracle_mean_headroom_pct": statistics.mean(r["oracle_headroom_pct"] for r in historical),
            "historical_oracle_mean_saving_us_op": statistics.mean(r["oracle_saving_us_op"] for r in historical),
            "oracle_reuse_status": "HISTORICAL_UNPAIRED", "classification": classification})
    write_csv(args.out_dir / "adaptive_scale_summary.csv", summary, list(summary[0]))
    write_csv(args.out_dir / "adaptive_scale_table.csv", summary, list(summary[0]))

    machines = sorted({r["machine"] for r in pairs})
    sources = sorted({r["source_id"] for r in pairs})
    report = args.out_dir / "adaptive_scale_verification.md"
    report.write_text(
        "# Adaptive-scale verification\n\n"
        "- New exact pairs: %d (50 per scale).\n"
        "- New machines: `%s`.\n- New source IDs: `%s`.\n"
        "- Correctness: zero misses and matching checksums/fingerprints within every pair.\n"
        "- Bootstrap: hierarchical paired bootstrap, %d draws, RNG seed %d.\n"
        "- Oracle: six-fanout static opportunity sweep, retained as `HISTORICAL_UNPAIRED`; "
        "it is not merged into exact new pairs because source/repetition identity differs.\n"
        "- Direct routing/controller timing is unavailable in the current schema. "
        "`derived_recurring_penalty_us_op` is the paired total latency penalty minus measured rebuild time per operation.\n"
        % (len(pairs), ",".join(machines), ",".join(sources), BOOTSTRAP_DRAWS, BOOTSTRAP_SEED),
        encoding="utf-8")


if __name__ == "__main__":
    main()
