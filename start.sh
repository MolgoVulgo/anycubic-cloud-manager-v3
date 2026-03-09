#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="${ROOT_DIR}/accloud"
BUILD_DIR="${APP_DIR}/build/dev-debug"
APP_BIN="${BUILD_DIR}/accloud_cli"
# Force explicit debug mode for all start.sh workflows.
export ACCLOUD_DEBUG=ON

usage() {
  cat <<'EOF'
Usage:
  ./start.sh
  ./start.sh 1 [-- <app_args...>]
  ./start.sh 2 [-- <app_args...>]

Modes:
  1 -> Compilation debug puis execution
  2 -> Execution debug seulement
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
  echo "  1) Compilation debug -> execution debug"
  echo "  2) Execution debug"
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
