"""Checkpointed single-host campaign runner. This module is not imported by the paper."""
import argparse
import csv
import hashlib
import itertools
import json
import os
import platform
import random
import subprocess
import sys
from pathlib import Path


def parameter_grid(spec):
    keys = ["records", "operations", "zipf", "read_update", "fanout",
            "phase_length", "hot_fraction", "sample_rate", "seed"]
    values = [spec.get(k, [None]) for k in keys]
    for combo in itertools.product(*values):
        yield dict(zip(keys, combo))


def machine_fingerprint():
    details = [platform.system(), platform.machine(), platform.processor(),
               str(os.cpu_count())]
    if sys.platform.startswith("linux"):
        try:
            cpuinfo = Path("/proc/cpuinfo").read_text(errors="replace")
            model = next((line.split(":", 1)[1].strip() for line in cpuinfo.splitlines()
                          if line.startswith("model name")), "UNKNOWN")
            details.append(model)
        except OSError:
            details.append("UNKNOWN")
    return hashlib.sha256("|".join(details).encode()).hexdigest()[:16], details


def source_fingerprint(root):
    h = hashlib.sha256()
    paths = [root / "benchmarks/benchmark_unified.cpp"] + sorted((root / "include").rglob("*.h"))
    for path in paths:
        h.update(str(path.relative_to(root)).encode())
        h.update(path.read_bytes())
    return h.hexdigest()[:20]


def git_commit(root):
    """Return archive-supplied or repository commit provenance."""
    supplied = os.environ.get("CABTREE_GIT_COMMIT", "").strip()
    if supplied:
        return supplied
    try:
        return subprocess.check_output(
            ["git", "-C", str(root), "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL, text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return "UNAVAILABLE_IN_SOURCE_ARCHIVE"


def append_csv_provenance(path, provenance):
    """Append immutable campaign provenance before atomic publication."""
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        rows = list(reader)
        fields = list(reader.fieldnames or [])
    if len(rows) != 1:
        raise ValueError("expected exactly one result row before publication: " + str(path))
    for name, value in provenance.items():
        if name not in fields:
            fields.append(name)
        rows[0][name] = value
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def measured_phase_targets(path, p, repetition, machine, source_id, spec):
    """Resolve an offline oracle choice for exactly this measured run."""
    mix = p["read_update"] or spec.get("default_read_update", [0.95, 0.05])
    expected = {
        "machine": machine, "source_id": source_id,
        "compiler_flags": os.environ.get("CABTREE_CXXFLAGS", "UNSUPPORTED"),
        "seed": int(p["seed"]), "repetition": int(spec.get("oracle_repetition", repetition)),
        "records": int(p["records"]), "operations": int(p["operations"]),
        "segments": int(spec.get("segments", 8)),
        "sample_rate": int(p["sample_rate"]),
        "phase_length": int(p["phase_length"]),
        "reads": float(mix[0]),
        "updates": float(mix[1]),
        "zipf": float(p["zipf"]),
        "hot_fraction": float(p["hot_fraction"]),
    }
    if not path.is_file():
        raise ValueError("measured phase-oracle CSV missing: " + str(path))
    matches = []
    with path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if row.get("variant") != "ORACLE-ZERO-COST":
                continue
            if spec.get("phase_oracle_family") and row.get("experiment_family") != spec["phase_oracle_family"]:
                continue
            try:
                aligned = all(
                    (float(row[k]) == value if isinstance(value, float) else
                     int(row[k]) == value if isinstance(value, int) else row[k] == value)
                    for k, value in expected.items()
                )
            except (KeyError, TypeError, ValueError):
                aligned = False
            if aligned:
                matches.append(row)
    if len(matches) != 1:
        raise ValueError("expected exactly one paired phase oracle for run; found " + str(len(matches)))
    targets = [int(x) for x in matches[0]["phase_fanouts"].split(";")]
    expected_phases = (int(p["operations"]) + int(p["phase_length"]) - 1) // int(p["phase_length"])
    candidates = set(map(int, spec.get("candidate_fanouts", [8, 16, 32, 64, 128, 256])))
    if len(targets) != expected_phases or not set(targets).issubset(candidates):
        raise ValueError("measured phase oracle has wrong phase count or fanout candidates")
    return targets


def command(binary, spec, p, variant, run_id, output, machine, source_id, repetition, cpu_model, environment,
            oracle_csv=None):
    args = [str(binary), "--variant", variant, "--run-id", run_id,
            "--output", str(output), "--family", spec.get("workload_family", "zipf"),
            "--experiment-family", spec["family"],
            "--machine", machine, "--source-id", source_id,
            "--repetition", str(repetition), "--cpu-model", cpu_model.replace(",", " "),
            "--environment", environment,
            "--compiler-flags", os.environ.get("CABTREE_CXXFLAGS", "UNSUPPORTED")]
    names = {"records": "records", "operations": "operations", "zipf": "zipf",
             "fanout": "fanout", "phase_length": "phase-length", "hot_fraction": "hot-fraction",
             "sample_rate": "sample-rate", "seed": "seed"}
    for key, option in names.items():
        if p[key] is not None:
            args.extend(["--" + option, str(p[key])])
    if p["read_update"] is not None:
        args.extend(["--reads", str(p["read_update"][0]),
                     "--updates", str(p["read_update"][1])])
    args.extend(["--segments", str(spec.get("segments", 8)),
                 "--candidate-fanouts", ":".join(map(str, spec.get("candidate_fanouts", [8,16,32,64,128,256]))),
                 "--warmup", str(spec.get("warmup", 0)),
                 "--latency-sampling-rate", str(spec.get("latency_sampling_rate", 128)),
                 "--adapt-interval", str(spec.get("adapt_interval", 5000))])
    if variant.startswith("PERFECT-"):
        targets = (measured_phase_targets(oracle_csv, p, repetition, machine, source_id, spec)
                   if oracle_csv is not None else spec.get("phase_fanouts", []))
        if not targets:
            raise ValueError("perfect detector requires measured phase fanouts")
        args.extend(["--phase-fanouts", ",".join(map(str, targets))])
    return args


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    parser.add_argument("--ordering-seed", type=int, default=2027)
    parser.add_argument("--source-id", default="AUTO")
    parser.add_argument("--phase-oracle-csv", type=Path,
                        help="Measured phase_oracle.py CSV for PERFECT variants")
    parser.add_argument("--execute", action="store_true", help="Explicitly run the campaign")
    args = parser.parse_args()
    spec = json.loads(args.config.read_text())
    if spec.get("selection_status") == "RESULT_PENDING":
        raise SystemExit("selected regimes are pending measured static results")
    oracle_csv = args.phase_oracle_csv or (Path(spec["phase_oracle_csv"])
                                            if spec.get("phase_oracle_csv") else None)
    if oracle_csv is not None and not oracle_csv.is_absolute():
        oracle_csv = Path(__file__).resolve().parents[1] / oracle_csv
    machine, details = machine_fingerprint()
    source_id = source_fingerprint(Path(__file__).resolve().parents[1]) if args.source_id == "AUTO" else args.source_id
    repository_root = Path(__file__).resolve().parents[1]
    commit = git_commit(repository_root)
    root = args.out_dir / machine / spec["family"]
    if args.execute:
        root.mkdir(parents=True, exist_ok=True)
        metadata = root / "environment.json"
        snapshot = {"machine_fingerprint": machine, "machine_components": details,
                    "source_id": source_id, "compiler_flags": os.environ.get("CABTREE_CXXFLAGS", "UNSUPPORTED"),
                    "ordering_seed": args.ordering_seed, "config": spec,
                    "binary": str(args.binary), "git_commit": commit,
                    "cpu_model": details[-1] if sys.platform.startswith("linux") else platform.processor()}
        if oracle_csv is not None:
            if not oracle_csv.is_file():
                raise SystemExit("measured phase-oracle CSV missing: " + str(oracle_csv))
            snapshot["phase_oracle_sha256"] = hashlib.sha256(oracle_csv.read_bytes()).hexdigest()
        if metadata.exists():
            if json.loads(metadata.read_text()) != snapshot:
                raise SystemExit("environment metadata changed; use a new campaign directory")
        else:
            metadata.write_text(json.dumps(snapshot, indent=2))
    run_count = 0
    for p in parameter_grid(spec):
        for repetition in range(spec.get("repetitions", 1)):
            variants = list(spec["variants"])
            order_key = json.dumps([p, repetition], sort_keys=True)
            rng = random.Random(args.ordering_seed ^ int(hashlib.sha256(order_key.encode()).hexdigest()[:12], 16))
            rng.shuffle(variants)
            for variant in variants:
                identity = json.dumps([spec["family"], p, repetition, variant, machine, source_id], sort_keys=True)
                run_id = hashlib.sha256(identity.encode()).hexdigest()[:24]
                output = root / (run_id + ".csv")
                partial = root / (run_id + ".partial.csv")
                environment = "kaggle" if "kaggle" in args.out_dir.parts else "local"
                cpu_model = details[-1] if sys.platform.startswith("linux") else (platform.processor() or "UNSUPPORTED")
                cmd = command(args.binary, spec, p, variant, run_id, partial, machine,
                              source_id, repetition, cpu_model, environment, oracle_csv)
                print(" ".join(cmd), flush=True)
                run_count += 1
                if not args.execute:
                    continue
                root.mkdir(parents=True, exist_ok=True)
                if output.exists():
                    print("SKIP completed", output, flush=True)
                    continue
                if partial.exists():
                    raise SystemExit("partial result exists; inspect before resuming: " + str(partial))
                log = root / (run_id + ".log")
                with log.open("w", encoding="utf-8") as stream:
                    stream.write("command: " + " ".join(cmd) + "\n")
                    stream.write("environment: " + json.dumps(details) + "\n")
                    stream.flush()
                    completed = subprocess.run(cmd, stdout=stream, stderr=subprocess.STDOUT,
                                               check=False)
                if completed.returncode != 0 or not partial.exists():
                    failure = root / (run_id + ".failed.json")
                    failure.write_text(json.dumps({"run_id": run_id,
                        "return_code": completed.returncode, "log": str(log),
                        "partial": str(partial), "command": cmd}, indent=2))
                    raise SystemExit("run failed; inspect " + str(log))
                sidecar = Path(str(partial) + ".phases.csv")
                if not sidecar.exists():
                    failure = root / (run_id + ".failed.json")
                    failure.write_text(json.dumps({"run_id": run_id,
                        "reason": "phase sidecar missing", "log": str(log)}, indent=2))
                    raise SystemExit("phase output missing; inspect " + str(log))
                provenance = {
                    "git_commit": commit,
                    "workload_family": spec.get("workload_family", "zipf"),
                    "adapt_interval": spec.get("adapt_interval", 5000),
                    "warmup": spec.get("warmup", 0),
                    "latency_sampling_rate": spec.get("latency_sampling_rate", 128),
                }
                append_csv_provenance(partial, provenance)
                append_csv_provenance(sidecar, provenance)
                sidecar.replace(Path(str(output) + ".phases.csv"))
                partial.replace(output)
    print("planned runs:", run_count)


if __name__ == "__main__":
    main()
