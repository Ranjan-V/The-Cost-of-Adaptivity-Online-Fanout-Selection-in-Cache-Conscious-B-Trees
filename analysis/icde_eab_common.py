"""Shared fail-closed readers and hierarchical paired bootstrap utilities."""
import csv
import random
import statistics
from collections import defaultdict
from pathlib import Path

IDENTITY = ("machine", "source_id", "compiler_flags", "experiment_family",
            "records", "operations", "zipf", "reads", "updates", "hot_fraction",
            "phase_length", "sample_rate", "segments", "fanout", "seed", "repetition")


def read_runs(root):
    rows = []
    failures = list(Path(root).rglob("*.failed.json"))
    partials = list(Path(root).rglob("*.partial.csv"))
    if failures or partials:
        raise ValueError("campaign contains failures/partials: %d/%d" % (len(failures), len(partials)))
    for path in Path(root).rglob("*.csv"):
        if path.name.endswith(".phases.csv"):
            continue
        with path.open(newline="", encoding="utf-8") as stream:
            found = list(csv.DictReader(stream))
        if len(found) != 1:
            raise ValueError("expected one row: " + str(path))
        row = found[0]; row["_path"] = str(path)
        if int(row["misses"]) != 0:
            raise ValueError("unexpected miss: " + str(path))
        rows.append(row)
    return rows


def exact_groups(rows, variants):
    groups = defaultdict(dict)
    for row in rows:
        key = tuple(row.get(k, "") for k in IDENTITY)
        if row["variant"] in groups[key]:
            raise ValueError("duplicate variant in exact group: " + str(key))
        groups[key][row["variant"]] = row
    complete = []
    for key, group in groups.items():
        if set(group) != set(variants):
            raise ValueError("incomplete group %s: %s" % (key, sorted(group)))
        if len({r["workload_fingerprint"] for r in group.values()}) != 1:
            raise ValueError("workload fingerprint mismatch: " + str(key))
        if len({r["checksum"] for r in group.values()}) != 1:
            raise ValueError("checksum mismatch: " + str(key))
        complete.append(group)
    return complete


def hierarchical_ci(values_by_seed, draws=20000, seed=20271017):
    """Resample workload seeds, then repetitions within each sampled seed."""
    rng = random.Random(seed); seeds = sorted(values_by_seed)
    if len(seeds) < 2:
        raise ValueError("hierarchical CI requires at least two seeds")
    observed = [x for s in seeds for x in values_by_seed[s]]
    boot = []
    for _ in range(draws):
        chosen = [rng.choice(seeds) for _ in seeds]
        sample = []
        for s in chosen:
            vals = values_by_seed[s]
            sample.extend(rng.choice(vals) for _ in vals)
        boot.append(statistics.mean(sample))
    boot.sort()
    return statistics.mean(observed), statistics.median(observed), boot[int(.025*draws)], boot[int(.975*draws)]


def write_csv(path, rows):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    if not rows:
        raise ValueError("refusing to write empty output")
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)
