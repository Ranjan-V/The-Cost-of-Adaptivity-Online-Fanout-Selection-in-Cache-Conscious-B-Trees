"""Apply the preregistered calibration rule and freeze Stage-C configs."""
import argparse
import json
from collections import defaultdict
from pathlib import Path
from icde_eab_common import hierarchical_ci, read_runs


def main():
    p = argparse.ArgumentParser(); p.add_argument("--raw", type=Path, required=True)
    p.add_argument("--root", type=Path, required=True); args = p.parse_args()
    rows = read_runs(args.raw)
    by = defaultdict(dict)
    for r in rows:
        key = (int(r["records"]), r["seed"])
        fanout = int(r["fanout"])
        if fanout in by[key]: raise ValueError("duplicate calibration fanout")
        by[key][fanout] = float(r["throughput_ops_sec"])
    records_values = sorted({k[0] for k in by})
    scored = []
    for records in records_values:
        seeds = sorted(k[1] for k in by if k[0] == records)
        if len(seeds) != 3 or any(set(by[(records, s)]) != {8,16,32,64,128,256} for s in seeds):
            raise ValueError("incomplete 3-seed calibration grid")
        values = {s: [max(by[(records,s)].values()) / by[(records,s)][64]] for s in seeds}
        mean, median, low, high = hierarchical_ci(values)
        scored.append((mean, low, records, median, high))
    scored.sort(reverse=True)
    winner = scored[0]
    report = {"selection_rule": "maximum mean best-static/f64 across the two frozen record counts",
              "eligibility_rule": "mean headroom >= 1.10 and no correctness failure",
              "scores": [{"records": x[2], "mean": x[0], "median": x[3], "ci_low": x[1], "ci_high": x[4]} for x in scored]}
    out = args.root / "results/processed/positive_control/calibration_selection.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    if winner[0] < 1.10:
        report["status"] = "NO_CREDIBLE_HIGH_HEADROOM_CONTROL"
        out.write_text(json.dumps(report, indent=2), encoding="utf-8")
        raise SystemExit("No candidate reaches the preregistered 10% mean-headroom threshold; do not run evaluation")
    report.update(status="SELECTED", records=winner[2])
    out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    for stem in ("positive_control_oracle_grid", "positive_control_evaluation"):
        template = args.root / ("experiments/configs/%s.template.json" % stem)
        spec = json.loads(template.read_text(encoding="utf-8"))
        spec["records"] = [winner[2]]; spec["selection_status"] = "FROZEN_FROM_CALIBRATION"
        (args.root / ("experiments/configs/%s.json" % stem)).write_text(json.dumps(spec, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__": main()
