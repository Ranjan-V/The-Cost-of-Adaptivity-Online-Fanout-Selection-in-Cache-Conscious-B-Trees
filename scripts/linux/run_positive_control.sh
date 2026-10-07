#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STAGE="${1:?stage required: calibration|oracle|evaluation}"
MODE="${2:-dry-run}"
case "$STAGE" in
  calibration) CONFIG=experiments/configs/positive_control_calibration.json ;;
  oracle) CONFIG=experiments/configs/positive_control_oracle_grid.json ;;
  evaluation) CONFIG=experiments/configs/positive_control_evaluation.json ;;
  *) echo "unknown stage: $STAGE" >&2; exit 2 ;;
esac
[[ -f "$ROOT/$CONFIG" ]] || { echo "Frozen config missing. Complete calibration selection first." >&2; exit 2; }
bash "$ROOT/scripts/linux/run_campaign.sh" "$CONFIG" "$MODE" positive_control
