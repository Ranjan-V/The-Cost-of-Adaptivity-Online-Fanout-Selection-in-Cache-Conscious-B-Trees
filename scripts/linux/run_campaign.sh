#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CONFIG="${1:?config path required}"
MODE="${2:-dry-run}"
DEST="${3:-kaggle}"
export CABTREE_CXXFLAGS='-std=c++11 -O3 -march=native -DNDEBUG'
args=(python3 "$ROOT/experiments/campaign.py" --config "$ROOT/$CONFIG" --binary "$ROOT/build/prepared/bench_unified" --out-dir "$ROOT/results/raw/$DEST" --source-id "${SOURCE_ID:-AUTO}")
if [[ "$MODE" == "--execute" ]]; then args+=(--execute); fi
printf '%q ' "${args[@]}"; echo
"${args[@]}"
