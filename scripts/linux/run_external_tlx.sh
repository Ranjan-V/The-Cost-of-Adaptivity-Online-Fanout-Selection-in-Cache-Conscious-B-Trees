#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MODE="${1:-dry-run}"
export CABTREE_CXXFLAGS='-std=c++11 -O3 -march=native -DNDEBUG'
[[ -s "$ROOT/build/prepared/external_source_id.txt" ]] || { echo "Missing external source identity; run build_icde_eab.sh" >&2; exit 2; }
SOURCE_ID="$(tr -d '[:space:]' < "$ROOT/build/prepared/external_source_id.txt")"
args=(python3 "$ROOT/experiments/campaign.py"
  --config "$ROOT/experiments/configs/external_tlx.json"
  --binary "$ROOT/build/prepared/bench_external_tlx"
  --source-file benchmarks/benchmark_external_tlx.cpp
  --source-id "$SOURCE_ID"
  --out-dir "$ROOT/results/raw/external_baseline")
[[ "$MODE" == "--execute" ]] && args+=(--execute)
printf '%q ' "${args[@]}"; echo
"${args[@]}"
