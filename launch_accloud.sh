#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="${ROOT_DIR}/accloud"
BUILD_ROOT="${APP_DIR}/build"
BUILD_DIR="${APP_DIR}/build/default"
APP_BIN="${BUILD_DIR}/accloud_cli"

usage() {
  cat <<'EOF'
Usage:
  ./launch_accloud.sh
  ./launch_accloud.sh 1 [-- <app_args...>]
  ./launch_accloud.sh 2 [-- <app_args...>]

Modes:
  1 -> Compilation puis execution
  2 -> Execution seulement
EOF
}

compile_app() {
  if [[ -d "${BUILD_ROOT}" ]]; then
    echo "[accloud] Nettoyage build: ${BUILD_ROOT}"
    rm -rf "${BUILD_ROOT}"
  fi

  echo "[accloud] Configuration CMake..."
  cmake --preset default
  echo "[accloud] Compilation..."
  cmake --build --preset default
}

run_app() {
  local args=("$@")

  if [[ ! -x "${APP_BIN}" ]]; then
    echo "[accloud] Binaire non trouve: ${APP_BIN}"
    echo "[accloud] Lance d'abord le mode 1 (compilation + execution)."
    exit 1
  fi

  echo "[accloud] Lancement: ${APP_BIN} ${args[*]:-}"
  exec "${APP_BIN}" "${args[@]}"
}

if [[ ! -d "${APP_DIR}" ]]; then
  echo "[accloud] Dossier introuvable: ${APP_DIR}"
  exit 1
fi

cd "${APP_DIR}"

mode="${1:-}"
if [[ "${mode}" == "-h" || "${mode}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ "${mode}" == "1" || "${mode}" == "2" ]]; then
  shift
else
  mode=""
fi

if [[ "${1:-}" == "--" ]]; then
  shift
fi
app_args=("$@")

if [[ -z "${mode}" ]]; then
  echo "Choisissez une option:"
  echo "  1) Compilation -> execution"
  echo "  2) Execution"
  read -r -p "Votre choix [1-2]: " mode
fi

case "${mode}" in
  1)
    compile_app
    run_app "${app_args[@]}"
    ;;
  2)
    run_app "${app_args[@]}"
    ;;
  *)
    echo "Choix invalide: ${mode}"
    usage
    exit 1
    ;;
esac
