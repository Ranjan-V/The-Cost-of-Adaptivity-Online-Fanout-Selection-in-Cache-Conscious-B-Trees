#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-$ROOT/kaggle_upload_adaptive_scale.zip}"
case "$OUT" in
  /*) ;;
  *) OUT="$PWD/$OUT" ;;
esac
COMMIT="$(git -C "$ROOT" rev-parse HEAD)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
TREE="$TMP/tree"
mkdir -p "$TREE"

# Explicit allowlist: package only files needed to build, execute, resume, and
# analyze this campaign. Copy from the current tree so prepared, uncommitted
# infrastructure is not silently replaced by HEAD.
copy_file() {
  mkdir -p "$TREE/$(dirname "$1")"
  cp "$ROOT/$1" "$TREE/$1"
}
copy_tree() {
  mkdir -p "$TREE/$1"
  cp -R "$ROOT/$1/." "$TREE/$1/"
}

copy_file "README.md"
copy_file "benchmarks/benchmark_unified.cpp"
copy_tree "include"
copy_file "tests/test_v2.cpp"
copy_file "tests/test_utils.h"
copy_file "experiments/campaign.py"
copy_file "experiments/configs/scale.json"
copy_file "scripts/linux/build_release.sh"
copy_file "scripts/linux/run_campaign.sh"
copy_file "scripts/linux/run_scale.sh"
copy_file "analysis/adaptive_scale.py"
copy_file "analysis/plot_adaptive_scale.py"
copy_file "docs/ADAPTIVE_SCALE_CAMPAIGN.md"
copy_file "kaggle/kaggle_adaptive_scale.ipynb"
copy_file "kaggle/KAGGLE_RUNBOOK.md"

printf '%s\n' "$COMMIT" > "$TREE/campaign_git_commit.txt"
DIRTY="no"
if ! git -C "$ROOT" diff --quiet -- || ! git -C "$ROOT" diff --cached --quiet -- ||
   [[ -n "$(git -C "$ROOT" ls-files --others --exclude-standard -- \
      analysis/adaptive_scale.py analysis/plot_adaptive_scale.py \
      docs/ADAPTIVE_SCALE_CAMPAIGN.md kaggle/kaggle_adaptive_scale.ipynb \
      scripts/linux/package_adaptive_scale.sh sigmod_submission/tables/adaptive_scale.tex)" ]]; then
  DIRTY="yes"
fi

cat > "$TREE/ADAPTIVE_SCALE_PACKAGE_MANIFEST.txt" <<EOF
Adaptive Scale Execution Package
Base Git commit: $COMMIT
Prepared working tree differs from base commit: $DIRTY
Campaign: adaptive_scale
Variants: STATIC fanout 64; ADAPT-V2
Scales: 100000, 1000000, 5000000 records
Seeds: 11, 23, 37, 53, 71
Repetitions: 10
Expected runs: 300 (150 STATIC, 150 ADAPT-V2)
Primary notebook: kaggle/kaggle_adaptive_scale.ipynb
Primary command: bash scripts/linux/run_scale.sh --execute kaggle
Raw output root: results/raw/kaggle/<machine-fingerprint>/adaptive_scale/
EOF

rm -f "$OUT"
(cd "$TREE" && zip -qr "$OUT" .)
printf 'Created %s from commit %s\n' "$OUT" "$COMMIT"
