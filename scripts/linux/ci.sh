#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MODE="${1:-gcc}"
case "$MODE" in
  gcc|normal) STAGE=gcc ;;
  clang) STAGE=clang ;;
  sanitizers|asan) STAGE=asan ;;
  concurrent) STAGE=concurrent ;;
  *) echo "Usage: $0 {gcc|clang|asan|concurrent}" >&2; exit 2 ;;
esac
exec bash "$ROOT/scripts/linux/run_codespaces_validation.sh" "$STAGE"
