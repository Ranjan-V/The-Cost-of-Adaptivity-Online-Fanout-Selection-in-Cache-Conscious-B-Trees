"""CPU-only Modal overflow screening. Dry-run by default; no final timing claims."""
from pathlib import Path
import hashlib
import itertools
import json
import os
import platform
import random
import subprocess

import modal


LOCAL_ROOT = Path(__file__).resolve().parents[1]
REMOTE_ROOT = Path("/opt/cabtree")
RESULT_ROOT = Path("/results")
VOLUME_NAME = "cabtree-overflow-results-v2"

image = (
    modal.Image.debian_slim(python_version="3.11")
    .apt_install("g++")
    .add_local_dir(LOCAL_ROOT / "include", REMOTE_ROOT / "include", copy=True)
    .add_local_dir(LOCAL_ROOT / "benchmarks", REMOTE_ROOT / "benchmarks", copy=True)
    .run_commands(
        "mkdir -p /opt/cabtree/build",
        "g++ -std=c++11 -Wall -Wextra -O3 -march=native -DNDEBUG "
        "/opt/cabtree/benchmarks/benchmark_unified.cpp "
        "-o /opt/cabtree/build/bench_unified",
    )
)

app = modal.App("cabtree-modal-overflow")
results = modal.Volume.from_name(VOLUME_NAME, create_if_missing=False)

AXES = ("records", "operations", "phase_length", "hot_fraction", "zipf",
        "read_update", "fanout", "sample_rate", "seed")


def source_id():
    digest = hashlib.sha256()
    paths = [LOCAL_ROOT / "benchmarks" / "benchmark_unified.cpp"]
    paths += sorted((LOCAL_ROOT / "include").rglob("*.h"))
    for path in paths:
        digest.update(str(path.relative_to(LOCAL_ROOT)).encode())
        digest.update(path.read_bytes())
    return digest.hexdigest()[:20]


def load_jobs(config_path):
    spec = json.loads(config_path.read_text(encoding="utf-8"))
    if spec.get("status") != "READY_FOR_REVIEW":
        raise ValueError("configuration must be READY_FOR_REVIEW")
    campaign_source = source_id()
    built = []
    for values in itertools.product(*(spec[key] for key in AXES)):
        parameters = dict(zip(AXES, values))
        for repetition in range(int(spec.get("repetitions", 1))):
            variants = list(spec["variants"])
            ordering = json.dumps([parameters, repetition], sort_keys=True)
            rng = random.Random(int(spec.get("ordering_seed", 2027)) ^
                                int(hashlib.sha256(ordering.encode()).hexdigest()[:12], 16))
            rng.shuffle(variants)
            for order, variant in enumerate(variants):
                payload = {"campaign": spec["campaign"], "variant": variant,
                           "variant_order": order, "repetition": repetition,
                           "parameters": parameters, "source_id": campaign_source,
                           "segments": int(spec.get("segments", 8)),
                           "adapt_interval": int(spec.get("adapt_interval", 5000)),
                           "warmup": int(spec.get("warmup", 0)),
                           "latency_sampling_rate": int(spec.get("latency_sampling_rate", 1024))}
                identity = json.dumps(payload, sort_keys=True, separators=(",", ":"))
                payload["job_id"] = hashlib.sha256(identity.encode()).hexdigest()[:24]
                built.append(payload)
    if len({job["job_id"] for job in built}) != len(built):
        raise ValueError("job-id collision")
    return spec, built


def cpu_model():
    try:
        for line in Path("/proc/cpuinfo").read_text(errors="replace").splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip().replace(",", " ")
    except OSError:
        pass
    return platform.processor() or "UNSUPPORTED"


def machine_id(model):
    components = [platform.system(), platform.machine(), model, str(os.cpu_count())]
    return "modal-" + hashlib.sha256("|".join(components).encode()).hexdigest()[:16]


@app.function(image=image, cpu=1.0, timeout=3600, volumes={str(RESULT_ROOT): results},
              max_containers=32)
def run_job(job):
    campaign_dir = RESULT_ROOT / "raw" / job["campaign"]
    campaign_dir.mkdir(parents=True, exist_ok=True)
    final = campaign_dir / (job["job_id"] + ".csv")
    partial = campaign_dir / (job["job_id"] + ".partial.csv")
    log = campaign_dir / (job["job_id"] + ".log")
    failure = campaign_dir / (job["job_id"] + ".failed.json")
    if final.exists():
        return {"job_id": job["job_id"], "status": "skipped"}
    if partial.exists():
        raise RuntimeError("partial output already exists: " + str(partial))

    p = job["parameters"]
    reads, updates = p["read_update"]
    model = cpu_model()
    machine = machine_id(model)
    command = [str(REMOTE_ROOT / "build" / "bench_unified"),
        "--variant", job["variant"], "--output", str(partial),
        "--run-id", job["job_id"], "--family", "shifting",
        "--experiment-family", job["campaign"], "--machine", machine,
        "--environment", "modal", "--source-id", job["source_id"],
        "--compiler-flags", "-std=c++11 -O3 -march=native -DNDEBUG",
        "--cpu-model", model, "--records", str(p["records"]),
        "--operations", str(p["operations"]), "--phase-length", str(p["phase_length"]),
        "--hot-fraction", str(p["hot_fraction"]), "--zipf", str(p["zipf"]),
        "--reads", str(reads), "--updates", str(updates), "--fanout", str(p["fanout"]),
        "--sample-rate", str(p["sample_rate"]), "--seed", str(p["seed"]),
        "--repetition", str(job["repetition"]), "--segments", str(job["segments"]),
        "--adapt-interval", str(job["adapt_interval"]), "--warmup", str(job["warmup"]),
        "--latency-sampling-rate", str(job["latency_sampling_rate"]), "--threads", "1",
        "--candidate-fanouts", "8:16:32:64:128:256"]
    with log.open("w", encoding="utf-8") as stream:
        stream.write("job: " + json.dumps(job, sort_keys=True) + "\n")
        stream.write("command: " + json.dumps(command) + "\n")
        completed = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
    sidecar = Path(str(partial) + ".phases.csv")
    if completed.returncode or not partial.exists() or not sidecar.exists():
        failure.write_text(json.dumps({"job": job, "return_code": completed.returncode,
            "partial_exists": partial.exists(), "sidecar_exists": sidecar.exists()}, indent=2))
        results.commit()
        raise RuntimeError("job failed: " + job["job_id"])
    sidecar.replace(Path(str(final) + ".phases.csv"))
    partial.replace(final)
    results.commit()
    return {"job_id": job["job_id"], "status": "completed"}


@app.local_entrypoint()
def main(config: str = "modal/configs/overflow_grid.json", execute: bool = False,
         max_jobs: int = 0):
    config_path = (LOCAL_ROOT / config).resolve()
    if LOCAL_ROOT not in config_path.parents:
        raise ValueError("config must be inside the repository")
    spec, jobs = load_jobs(config_path)
    if max_jobs > 0:
        jobs = jobs[:max_jobs]
    print("campaign:", spec["campaign"])
    print("planned jobs:", len(jobs))
    print("CPU only; max containers: 32; result volume:", VOLUME_NAME)
    if not execute:
        print("DRY RUN: no remote functions invoked. Pass --execute to submit.")
        return
    completed = skipped = 0
    for outcome in run_job.map(jobs):
        completed += outcome["status"] == "completed"
        skipped += outcome["status"] == "skipped"
        if (completed + skipped) % 100 == 0:
            print("progress:", completed, "completed,", skipped, "skipped")
    print("finished:", completed, "completed,", skipped, "skipped")
