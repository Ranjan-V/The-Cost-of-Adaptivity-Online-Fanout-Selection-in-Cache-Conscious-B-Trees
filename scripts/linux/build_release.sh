#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CXX="${CXX:-g++}"
RUN="${1:-dry-run}"
mkdir_cmd=(mkdir -p "$ROOT/build/prepared")
benchmark=("$CXX" -std=c++11 -Wall -Wextra -O3 -march=native -DNDEBUG "$ROOT/benchmarks/benchmark_unified.cpp" -o "$ROOT/build/prepared/bench_unified")
testcmd=("$CXX" -std=c++11 -Wall -Wextra -O0 -g "$ROOT/tests/test_v2.cpp" -o "$ROOT/build/prepared/test_v2")
printf '%q ' "${benchmark[@]}"; echo
printf '%q ' "${testcmd[@]}"; echo
if [[ "$RUN" == "--execute" ]]; then
  "${mkdir_cmd[@]}"
  "${benchmark[@]}" >"$ROOT/build/prepared/bench_unified.build.log" 2>&1 || { cat "$ROOT/build/prepared/bench_unified.build.log"; exit 1; }
  "${testcmd[@]}" >"$ROOT/build/prepared/test_v2.build.log" 2>&1 || { cat "$ROOT/build/prepared/test_v2.build.log"; exit 1; }
fi
