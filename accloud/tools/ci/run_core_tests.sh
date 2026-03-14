#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${ROOT_DIR}"

echo "[ci] configure default preset"
cmake --preset default

echo "[ci] build core targets"
cmake --build --preset default --target \
  accloud_cli \
  accloud_har_tests \
  accloud_mqtt_tests \
  accloud_log_tests \
  accloud_cloud_core_tests \
  accloud_security_tests

echo "[ci] run regression guard tests"
TEST_REGEX="accloud_har_import|accloud_mqtt_flow|accloud_log_flow|accloud_cloud_core_regressions|accloud_security_redaction"
if ! ctest --preset default -R "${TEST_REGEX}" --output-on-failure; then
  echo "[ci] first test run failed, retrying once after short backoff..."
  sleep 1
  ctest --preset default -R "${TEST_REGEX}" --output-on-failure
fi
