#!/usr/bin/env bash
set -euo pipefail
bash "$(dirname "$0")/run_campaign.sh" experiments/configs/v2_component_ablation.json "${1:-dry-run}" "${2:-v2_overhead}"
