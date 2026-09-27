#!/usr/bin/env bash
set -euo pipefail
bash "$(dirname "$0")/run_campaign.sh" experiments/configs/monitor_ladder.json "${1:-dry-run}" "${2:-kaggle}"
