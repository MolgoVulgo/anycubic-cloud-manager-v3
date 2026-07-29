#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${ROOT_DIR}/accloud"

CONFIGURE_ARGS=()
DEFAULT_ARCHIVE="${ROOT_DIR}/accloud-build-deps.zip"
if [[ -n "${ACCLOUD_DEPENDENCY_ARCHIVE:-}" ]]; then
  CONFIGURE_ARGS+=("-DACCLOUD_DEPENDENCY_ARCHIVE=${ACCLOUD_DEPENDENCY_ARCHIVE}")
elif [[ -f "${DEFAULT_ARCHIVE}" ]]; then
  CONFIGURE_ARGS+=("-DACCLOUD_DEPENDENCY_ARCHIVE=${DEFAULT_ARCHIVE}")
fi

echo "[ci] configure protected-core preset"
cmake --preset protected-core "${CONFIGURE_ARGS[@]}"

echo "[ci] build protected core"
cmake --build --preset protected-core

echo "[ci] run protected core and static guards"
ctest --preset protected-core --output-on-failure
