#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="${ROOT_DIR}/accloud"
BUILD_DIR="${APP_DIR}/build/dev-debug"
APP_BIN="${BUILD_DIR}/accloud_cli"
PROD_BUILD_DIR="${APP_DIR}/build/prod"
PROD_APP_BIN="${PROD_BUILD_DIR}/accloud_cli"
# Force explicit debug mode for all start.sh workflows.
export ACCLOUD_DEBUG=ON

usage() {
  cat <<'EOF'
Usage:
  ./start.sh              # defaults to mode 2
  ./start.sh 1 [-- <app_args...>]
  ./start.sh 2 [-- <app_args...>]
  ./start.sh 3

Modes:
  1 -> Compilation debug puis execution
  2 -> Execution debug seulement
  3 -> Compilation prod puis execution
Environment:
  ACCLOUD_DEBUG is forced to ON by this script.
EOF
}

compile_app() {
  if [[ -d "${BUILD_DIR}" ]]; then
    echo "[accloud] Nettoyage build debug: ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
  fi

  echo "[accloud] Configuration CMake (dev-debug)..."
  cmake --preset dev-debug -DACCLOUD_DEBUG=ON
  echo "[accloud] Compilation (dev-debug)..."
  cmake --build --preset dev-debug
}

compile_prod() {
  if [[ -d "${PROD_BUILD_DIR}" ]]; then
    echo "[accloud] Nettoyage build prod: ${PROD_BUILD_DIR}"
    rm -rf "${PROD_BUILD_DIR}"
  fi

  echo "[accloud] Configuration CMake (prod)..."
  cmake --preset prod -DACCLOUD_DEBUG=OFF
  echo "[accloud] Compilation (prod)..."
  cmake --build --preset prod
}

run_prod_app() {
  local args=("$@")

  if [[ ! -x "${PROD_APP_BIN}" ]]; then
    echo "[accloud] Binaire prod non trouve: ${PROD_APP_BIN}"
    echo "[accloud] Lance d'abord le mode 3 (compilation prod + execution)."
    exit 1
  fi

  echo "[accloud] Lancement prod: ${PROD_APP_BIN} ${args[*]:-}"
  exec "${PROD_APP_BIN}" "${args[@]}"
}

run_app() {
  local args=("$@")

  if [[ ! -x "${APP_BIN}" ]]; then
    echo "[accloud] Binaire non trouve: ${APP_BIN}"
    echo "[accloud] Lance d'abord le mode 1 (compilation + execution)."
    exit 1
  fi

  local final_args=()
  local has_debug_ui=0
  for arg in "${args[@]}"; do
    if [[ "${arg}" == "--debug-ui" ]]; then
      has_debug_ui=1
      break
    fi
  done

  if [[ ${has_debug_ui} -eq 0 ]]; then
    final_args+=("--debug-ui")
  fi
  final_args+=("${args[@]}")

  echo "[accloud] Lancement debug: ${APP_BIN} ${final_args[*]:-}"
  exec "${APP_BIN}" "${final_args[@]}"
}

if [[ ! -d "${APP_DIR}" ]]; then
  echo "[accloud] Dossier introuvable: ${APP_DIR}"
  exit 1
fi

cd "${APP_DIR}"

arg_count=$#
mode="${1:-}"
if [[ "${mode}" == "-h" || "${mode}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ "${mode}" == "1" || "${mode}" == "2" || "${mode}" == "3" ]]; then
  shift
else
  mode=""
fi

if [[ "${1:-}" == "--" ]]; then
  shift
fi
app_args=("$@")

if [[ -z "${mode}" ]]; then
  if [[ ${arg_count} -eq 0 ]]; then
    mode="2"
  else
    echo "Choisissez une option:"
    echo "  1) Compilation debug -> execution debug"
    echo "  2) Execution debug"
    echo "  3) Compilation prod -> execution prod"
    read -r -p "Votre choix [1-3]: " mode
  fi
fi

case "${mode}" in
  1)
    compile_app
    run_app "${app_args[@]}"
    ;;
  2)
    run_app "${app_args[@]}"
    ;;
  3)
    compile_prod
    run_prod_app "${app_args[@]}"
    ;;
  *)
    echo "Choix invalide: ${mode}"
    usage
    exit 1
    ;;
esac
