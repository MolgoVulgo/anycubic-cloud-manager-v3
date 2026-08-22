#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/accloud"
TMP_OUTPUT="$(mktemp)"

cleanup() {
  rm -f "$TMP_OUTPUT"
}
trap cleanup EXIT
trap 'status=$?; printf "\nERREUR: commande en échec (code %d): %s\n" "$status" "$BASH_COMMAND" >&2; exit "$status"' ERR

cd "$BUILD_DIR"

printf '== Configuration experimental-viewer-qt ==\n'
cmake --preset experimental-viewer-qt

printf '\n== Build experimental-viewer-qt ==\n'
cmake --build --preset experimental-viewer-qt --clean-first

require_test() {
  local test_name="$1"
  if ! ctest --preset experimental-viewer-qt -N -R "^${test_name}$" \
      | grep -Fq "${test_name}"; then
    printf 'ERREUR: le test requis %s n est pas déclaré dans experimental-viewer-qt.\n' \
      "$test_name" >&2
    exit 2
  fi
}

run_required_test() {
  local test_name="$1"
  printf '\n== Test %s ==\n' "$test_name"
  : > "$TMP_OUTPUT"
  set +e
  ctest --preset experimental-viewer-qt \
    -R "^${test_name}$" \
    --output-on-failure 2>&1 | tee "$TMP_OUTPUT"
  local status=${PIPESTATUS[0]}
  set -e

  if (( status != 0 )); then
    printf 'ERREUR: %s a échoué (code %d).\n' "$test_name" "$status" >&2
    exit "$status"
  fi

  if grep -Eq '(\*+Skipped|[[:space:]]Skipped[[:space:]]|No tests were found)' "$TMP_OUTPUT"; then
    printf 'ERREUR: %s n a pas été exécuté complètement (Skipped ou absent).\n' \
      "$test_name" >&2
    exit 3
  fi
}

require_test accloud_render3d_shader_compile
require_test accloud_support_compute_vulkan

run_required_test accloud_render3d_shader_compile
run_required_test accloud_support_compute_vulkan

printf '\n== Gate complet experimental-viewer-qt ==\n'
ctest --preset experimental-viewer-qt --output-on-failure

printf '\nValidation utilisateur Qt/OpenGL/Vulkan réussie.\n'
