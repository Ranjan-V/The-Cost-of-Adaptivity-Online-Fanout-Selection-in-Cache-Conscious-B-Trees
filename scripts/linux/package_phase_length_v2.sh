#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-$ROOT/kaggle_upload_phase_length_v2.zip}"
case "$OUT" in /*) ;; *) OUT="$PWD/$OUT" ;; esac
COMMIT="$(git -C "$ROOT" rev-parse HEAD)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
TREE="$TMP/tree"
mkdir -p "$TREE"

copy_file() { mkdir -p "$TREE/$(dirname "$1")"; cp "$ROOT/$1" "$TREE/$1"; }
copy_tree() { mkdir -p "$TREE/$1"; cp -R "$ROOT/$1/." "$TREE/$1/"; }

copy_file "README.md"
copy_file "benchmarks/benchmark_unified.cpp"
copy_tree "include"
copy_file "tests/test_v2.cpp"
copy_file "tests/test_utils.h"
copy_file "experiments/campaign.py"
copy_file "experiments/configs/phase_length_v2_oracle_grid.json"
copy_file "experiments/configs/phase_length_v2.json"
copy_file "scripts/linux/build_release.sh"
copy_file "scripts/linux/run_campaign.sh"
copy_file "scripts/linux/run_phase_length_v2_oracle.sh"
copy_file "scripts/linux/run_phase_length_v2.sh"
copy_file "analysis/phase_oracle.py"
copy_file "analysis/phase_length_break_even.py"
copy_file "docs/PHASE_LENGTH_V2_CAMPAIGN.md"
copy_file "kaggle/kaggle_phase_length_v2.ipynb"

printf '%s\n' "$COMMIT" > "$TREE/campaign_git_commit.txt"
cat > "$TREE/PHASE_LENGTH_V2_PACKAGE_MANIFEST.txt" <<EOF
Phase-Length V2 Execution Package
Base Git commit: $COMMIT
Stage A: 150 STATIC-REGIONAL oracle-grid runs
Stage B: 375 STATIC-REGIONAL/PERFECT-V2/ADAPT-V2 runs
Total expected runs: 525
Accelerator: None (CPU only)
Notebook: kaggle/kaggle_phase_length_v2.ipynb
EOF

rm -f "$OUT"
(cd "$TREE" && zip -qr "$OUT" .)
printf 'Created %s from commit %s\n' "$OUT" "$COMMIT"
