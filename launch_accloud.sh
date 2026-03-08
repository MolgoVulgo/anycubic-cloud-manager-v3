#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
START_SCRIPT="${ROOT_DIR}/start.sh"

if [[ ! -x "${START_SCRIPT}" ]]; then
  echo "[accloud] Script introuvable/non executable: ${START_SCRIPT}"
  echo "[accloud] Lance: chmod +x start.sh"
  exit 1
fi

echo "[accloud] launch_accloud.sh delegue maintenant a start.sh (mode debug)."
exec "${START_SCRIPT}" "$@"
