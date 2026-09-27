"""Validate and summarize a Modal grid without importing Modal or running compute."""
import argparse
import hashlib
import itertools
import json
from pathlib import Path


AXES = ("records", "operations", "phase_length", "hot_fraction", "zipf",
        "read_update", "fanout", "sample_rate", "seed")


def load(path):
    spec = json.loads(path.read_text(encoding="utf-8"))
    required = {"campaign", "status", "workload_family", "variants", "repetitions"}
    missing = sorted(required - set(spec))
    if missing:
        raise ValueError("missing fields: " + ", ".join(missing))
    if spec["status"] != "READY_FOR_REVIEW":
        raise ValueError("campaign status is not READY_FOR_REVIEW")
    for axis in AXES:
        if not isinstance(spec.get(axis), list) or not spec[axis]:
            raise ValueError(axis + " must be a nonempty list")
    if not spec["variants"] or int(spec["repetitions"]) < 1:
        raise ValueError("variants/repetitions are invalid")
    return spec


def jobs(spec):
    for values in itertools.product(*(spec[k] for k in AXES)):
        parameters = dict(zip(AXES, values))
        for repetition in range(int(spec["repetitions"])):
            for variant in spec["variants"]:
                payload = {"campaign": spec["campaign"], "variant": variant,
                           "repetition": repetition, "parameters": parameters}
                identity = json.dumps(payload, sort_keys=True, separators=(",", ":"))
                payload["job_id"] = hashlib.sha256(identity.encode()).hexdigest()[:24]
                yield payload


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()
    spec = load(args.config)
    planned = list(jobs(spec))
    ids = [j["job_id"] for j in planned]
    if len(ids) != len(set(ids)):
        raise ValueError("job-id collision")
    print("campaign:", spec["campaign"])
    print("planned jobs:", len(planned))
    for axis in AXES:
        print("  {}: {}".format(axis, len(spec[axis])))
    print("  variants:", len(spec["variants"]))
    print("  repetitions:", spec["repetitions"])
    if args.manifest:
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_text(json.dumps({"config": spec, "job_count": len(planned),
            "config_sha256": hashlib.sha256(args.config.read_bytes()).hexdigest(),
            "first_jobs": planned[:10]}, indent=2), encoding="utf-8")
        print("manifest:", args.manifest)


if __name__ == "__main__":
    main()
