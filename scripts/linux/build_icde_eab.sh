#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CXX="${CXX:-g++}"
MODE="${1:-all}"
[[ "$MODE" == "all" || "$MODE" == "unified" || "$MODE" == "external" ]] || { echo "usage: $0 [all|unified|external]" >&2; exit 2; }
FLAGS=(-std=c++11 -Wall -Wextra -Wpedantic -Werror -O3 -march=native -DNDEBUG)
mkdir -p "$ROOT/build/prepared"
if [[ "$MODE" == "all" || "$MODE" == "unified" ]]; then
  "$CXX" "${FLAGS[@]}" "$ROOT/benchmarks/benchmark_unified.cpp" -o "$ROOT/build/prepared/bench_unified"
fi
[[ "$MODE" == "unified" ]] && exit 0

TLX="$ROOT/third_party/tlx/src"
[[ -f "$TLX/tlx/container/btree_map.hpp" ]] || { echo "Missing pinned TLX checkout; see third_party/tlx/PROVENANCE.md" >&2; exit 2; }
[[ -f "$ROOT/third_party/tlx/RESOLVED_COMMIT.txt" ]] || { echo "Missing TLX resolved commit record" >&2; exit 2; }
[[ -f "$TLX/LICENSE" || -f "$TLX/LICENSE_1_0.txt" ]] || { echo "Missing TLX license" >&2; exit 2; }
[[ -z "$(git -C "$TLX" status --porcelain)" ]] || { echo "TLX checkout is modified" >&2; exit 2; }
git -C "$TLX" describe --tags --exact-match | grep -qx 'v0.6.1' || { echo "TLX checkout is not tag v0.6.1" >&2; exit 2; }
TLX_COMMIT="$(git -C "$TLX" rev-parse HEAD)"
[[ "$TLX_COMMIT" == "$(tr -d '[:space:]' < "$ROOT/third_party/tlx/RESOLVED_COMMIT.txt")" ]] || { echo "TLX commit record mismatch" >&2; exit 2; }
"$CXX" "${FLAGS[@]}" -I"$TLX" "-DCABTREE_TLX_COMMIT=\"$TLX_COMMIT\"" \
  "$ROOT/benchmarks/benchmark_external_tlx.cpp" -o "$ROOT/build/prepared/bench_external_tlx"
PROJECT_HEADERS="$(find "$ROOT/include" -type f -name '*.h' -print0 | sort -z | xargs -0 sha256sum | sha256sum | cut -d' ' -f1)"
SOURCE_ID="$(printf '%s\n' "$TLX_COMMIT" "$PROJECT_HEADERS" "$(sha256sum "$ROOT/benchmarks/benchmark_external_tlx.cpp" | cut -d' ' -f1)" | sha256sum | cut -c1-20)"
printf '%s\n' "$SOURCE_ID" > "$ROOT/build/prepared/external_source_id.txt"
