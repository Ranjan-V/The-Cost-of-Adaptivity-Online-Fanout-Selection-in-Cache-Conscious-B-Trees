#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cmd=(python3 "$ROOT/analysis/aggregate.py" --raw "$ROOT/results/raw" --output "$ROOT/results/processed/summary.csv")
printf '%q ' "${cmd[@]}"; echo
if [[ "${1:-dry-run}" == "--execute" ]]; then "${cmd[@]}"; fi
