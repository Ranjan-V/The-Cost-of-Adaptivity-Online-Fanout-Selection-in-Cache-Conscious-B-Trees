#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STAGE="${1:-}"
DRY_RUN=0
if [[ "$STAGE" == "--dry-run" ]]; then STAGE="${2:-}"; DRY_RUN=1; fi
if [[ "${2:-}" == "--dry-run" ]]; then DRY_RUN=1; fi

case "$STAGE" in
  env|gcc|clang|tests|asan|tsan|concurrent) ;;
  *) echo "Usage: $0 [--dry-run] {env|gcc|clang|tests|asan|tsan|concurrent} [--dry-run]" >&2; exit 2 ;;
esac

STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
RUN_DIR="$ROOT/results/validation/$STAMP-$STAGE"
LOG_DIR="$RUN_DIR/logs"
ENV_DIR="$RUN_DIR/environment"

print_command() { printf '+ '; printf '%q ' "$@"; printf '\n'; }
run_logged() {
  local name="$1"; shift
  print_command "$@"
  if [[ "$DRY_RUN" -eq 0 ]]; then
    mkdir -p "$LOG_DIR"
    "$@" 2>&1 | tee "$LOG_DIR/$name.log"
  fi
}
run_with_env() {
  local name="$1"; shift
  print_command env "$@"
  if [[ "$DRY_RUN" -eq 0 ]]; then
    mkdir -p "$LOG_DIR"
    env "$@" 2>&1 | tee "$LOG_DIR/$name.log"
  fi
}
run_environment() {
  local name="$1"; shift
  print_command "$@"
  if [[ "$DRY_RUN" -eq 0 ]]; then
    mkdir -p "$ENV_DIR"
    "$@" >"$ENV_DIR/$name.txt" 2>&1
    cat "$ENV_DIR/$name.txt"
  fi
}

if [[ "$DRY_RUN" -eq 0 ]]; then
  mkdir -p "$LOG_DIR" "$ENV_DIR"
  printf '%s\n' "$STAGE" >"$RUN_DIR/stage.txt"
fi

if [[ "$STAGE" != "env" ]]; then
  run_environment uname uname -a
  run_environment lscpu lscpu
  run_environment gcc-version g++ --version
  run_environment clang-version clang++ --version
  run_environment cmake-version cmake --version
  run_environment ninja-version ninja --version
  run_environment git-version git --version
fi

case "$STAGE" in
  env)
    run_environment uname uname -a
    run_environment lscpu lscpu
    run_environment gcc-version g++ --version
    run_environment clang-version clang++ --version
    run_environment cmake-version cmake --version
    run_environment ninja-version ninja --version
    run_environment git-version git --version
    ;;
  gcc)
    run_logged configure cmake --preset gcc-release
    run_logged build cmake --build --preset gcc-release --parallel
    run_logged tests ctest --preset gcc-release
    ;;
  clang)
    run_logged configure cmake --preset clang-release
    run_logged build cmake --build --preset clang-release --parallel
    run_logged tests ctest --preset clang-release
    ;;
  tests)
    run_logged configure cmake --preset gcc-debug
    run_logged build cmake --build --preset gcc-debug --parallel
    run_logged tests ctest --preset gcc-debug
    ;;
  asan)
    run_logged configure cmake --preset asan-ubsan
    run_logged build cmake --build --preset asan-ubsan --parallel
    run_with_env tests \
      ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1 \
      UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
      ctest --preset asan-ubsan
    ;;
  tsan)
    run_logged configure cmake --preset tsan
    run_logged build cmake --build --preset tsan --parallel
    run_with_env concurrent TSAN_OPTIONS=halt_on_error=1:history_size=7 \
      "$ROOT/build/codespaces/tsan/test_btree_concurrent_correctness" \
      "$RUN_DIR/concurrent_correctness.csv"
    ;;
  concurrent)
    run_logged configure cmake --preset clang-debug
    run_logged build cmake --build --preset clang-debug --target test_btree_concurrent_correctness --parallel
    run_logged concurrent "$ROOT/build/codespaces/clang-debug/test_btree_concurrent_correctness" \
      "$RUN_DIR/concurrent_correctness.csv"
    ;;
esac

if [[ "$DRY_RUN" -eq 0 ]]; then
  printf 'PASS\n' >"$RUN_DIR/stage_status.txt"
  echo "Validation evidence: $RUN_DIR"
fi
