#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-$ROOT/icde_eab_strengthening_prepared.zip}"
case "$OUT" in /*) ;; *) OUT="$PWD/$OUT" ;; esac
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
TREE="$TMP/icde_eab_strengthening"
mkdir -p "$TREE"
copy_file() { mkdir -p "$TREE/$(dirname "$1")"; cp "$ROOT/$1" "$TREE/$1"; }
copy_tree() { mkdir -p "$TREE/$1"; cp -R "$ROOT/$1/." "$TREE/$1/"; }

copy_file README.md
copy_file benchmarks/benchmark_unified.cpp
copy_file benchmarks/benchmark_external_tlx.cpp
copy_tree include
copy_file experiments/campaign.py
copy_file experiments/RESULT_SCHEMA.md
for f in v2_component_ablation external_tlx positive_control_calibration; do copy_file "experiments/configs/$f.json"; done
copy_file experiments/configs/positive_control_evaluation.template.json
copy_file experiments/configs/positive_control_oracle_grid.template.json
for f in icde_eab_common v2_component_ablation external_tlx select_positive_control positive_control phase_oracle; do copy_file "analysis/$f.py"; done
for f in build_icde_eab run_campaign run_v2_component_ablation run_external_tlx run_positive_control; do copy_file "scripts/linux/$f.sh"; done
copy_file third_party/tlx/PROVENANCE.md
for f in ICDE_EAB_STRENGTHENING_PLAN V2_COMPONENT_ABLATION EXTERNAL_INDEX_BASELINE POSITIVE_CONTROL_BREAK_EVEN; do copy_file "docs/$f.md"; done
copy_tree paper_scaffolds
git -C "$ROOT" rev-parse HEAD > "$TREE/PREPARED_FROM_COMMIT.txt"
git -C "$ROOT" diff --binary | sha256sum | cut -d' ' -f1 > "$TREE/UNCOMMITTED_DIFF_SHA256.txt"
cat > "$TREE/PACKAGE_STATUS.txt" <<'EOF'
Implementation and preparation only. No benchmark result is included.
Maximum planned executions: 436 (150 V2 ablation, 50 TLX, 36 calibration,
and 200 positive-control evaluation only if calibration eligibility succeeds).
TLX source is intentionally absent; follow third_party/tlx/PROVENANCE.md.
EOF
rm -f "$OUT"
(cd "$TMP" && zip -qr "$OUT" icde_eab_strengthening)
printf 'Created packaging-only archive: %s\n' "$OUT"
