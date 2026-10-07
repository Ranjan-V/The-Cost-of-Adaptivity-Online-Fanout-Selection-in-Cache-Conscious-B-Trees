#!/usr/bin/env bash
set -euo pipefail
bash "$(dirname "$0")/run_campaign.sh" \
  experiments/configs/phase_length_v2.json \
  "${1:-dry-run}" "${2:-kaggle}"
