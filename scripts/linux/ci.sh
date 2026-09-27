#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CXX="${CXX:-g++}"
MODE="${1:-normal}"
BUILD="$ROOT/build/ci-${CXX##*/}-${MODE}"
mkdir -p "$BUILD"

common=(-std=c++11 -Wall -Wextra -Werror -pedantic -I"$ROOT/include")
link_flags=()
if [[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* ]]; then
  link_flags+=(-lpsapi)
else
  link_flags+=(-pthread)
fi
if [[ "$MODE" == "sanitizers" ]]; then
  common+=(-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined)
else
  common+=(-O2)
fi

tests=(
  test_btree
  test_monitor
  test_predictor
  test_adaptive
  test_adaptive_regression
  test_adaptive_aco
  test_segmented_adaptive
  test_v2
  simple_test
)

for name in "${tests[@]}"; do
  echo "[build] $name with $CXX ($MODE)"
  "$CXX" "${common[@]}" "$ROOT/tests/$name.cpp" -o "$BUILD/$name" "${link_flags[@]}"
done

echo "[build] unified benchmark"
"$CXX" "${common[@]}" "$ROOT/benchmarks/benchmark_unified.cpp" \
  -o "$BUILD/bench_unified" "${link_flags[@]}"

for name in "${tests[@]}"; do
  echo "[test] $name"
  "$BUILD/$name"
done

echo "[smoke] STATIC / ADAPT-V1 / ADAPT-V2"
mkdir -p "$BUILD/smoke"
for variant in STATIC ADAPT-V1 ADAPT-V2; do
  safe="$(printf '%s' "$variant" | tr '[:upper:]-' '[:lower:]_')"
  "$BUILD/bench_unified" \
    --variant "$variant" --output "$BUILD/smoke/$safe.csv" \
    --run-id "ci-$safe" --family shifting \
    --experiment-family ci-correctness --machine github-actions \
    --environment github-actions --source-id ci \
    --compiler-flags "$CXX-${common[*]}" --cpu-model github-runner \
    --records 10000 --operations 100000 --segments 8 \
    --phase-length 20000 --adapt-interval 5000 --sample-rate 32 \
    --warmup 1000 --latency-sampling-rate 128 --threads 1 \
    --repetition 0 --fanout 64 --seed 20260921 --zipf 0.99 \
    --reads 0.95 --updates 0.05 --hot-fraction 0.20 --hot-segment 0 \
    --candidate-fanouts 8:16:32:64:128:256
done

python3 - "$BUILD/smoke" <<'PY'
import csv
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
rows = []
for name in ("static.csv", "adapt_v1.csv", "adapt_v2.csv"):
    with (root / name).open(newline="", encoding="utf-8") as stream:
        rows.extend(csv.DictReader(stream))
assert len(rows) == 3, "expected three smoke rows"
assert len({r["checksum"] for r in rows}) == 1, "checksum mismatch"
assert len({r["workload_fingerprint"] for r in rows}) == 1, "fingerprint mismatch"
assert all(int(r["misses"]) == 0 for r in rows), "lookup miss"
print("CI smoke agreement:", rows[0]["checksum"], rows[0]["workload_fingerprint"])
PY

python3 -m py_compile \
  "$ROOT/experiments/campaign.py" \
  "$ROOT/analysis/aggregate.py" \
  "$ROOT/analysis/paired.py" \
  "$ROOT/modal/plan.py" \
  "$ROOT/modal/modal_campaign.py"
python3 "$ROOT/modal/plan.py" \
  --config "$ROOT/modal/configs/overflow_grid.json" \
  --manifest "$BUILD/modal-plan.json"

echo "CI completed successfully."
