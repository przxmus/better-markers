#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_SCRIPT="$ROOT_DIR/scripts/dev/build-install.sh"
CONFIGURE_SCRIPT="$ROOT_DIR/scripts/dev/configure.sh"
BUILD_DIR="$ROOT_DIR/build_macos"
WATCH_PATHS=(
  "$ROOT_DIR/src"
  "$ROOT_DIR/data"
  "$ROOT_DIR/cmake"
  "$ROOT_DIR/CMakeLists.txt"
  "$ROOT_DIR/CMakePresets.json"
  "$ROOT_DIR/buildspec.json"
)
CONFIG_PATHS=(
  "$ROOT_DIR/cmake"
  "$ROOT_DIR/CMakeLists.txt"
  "$ROOT_DIR/CMakePresets.json"
  "$ROOT_DIR/buildspec.json"
)

last_config_state=""

config_state() {
  {
    for path in "${CONFIG_PATHS[@]}"; do
      if [ -d "$path" ]; then
        find "$path" -type f -print0
      elif [ -f "$path" ]; then
        printf '%s\0' "$path"
      fi
    done
  } | xargs -0 stat -f '%m %N' 2>/dev/null | sort | shasum | awk '{print $1}'
}

ensure_configured() {
  local current_state
  current_state="$(config_state)"

  if [ ! -d "$BUILD_DIR" ] || [ "$current_state" != "$last_config_state" ]; then
    echo "[$(date '+%H:%M:%S')] Config changed. Reconfiguring..."
    "$CONFIGURE_SCRIPT"
    last_config_state="$current_state"
  fi
}

run_build() {
  echo "[$(date '+%H:%M:%S')] Rebuilding and installing..."
  if ensure_configured && "$BUILD_SCRIPT"; then
    echo "[$(date '+%H:%M:%S')] Build/install complete. Restart OBS to reload plugin binaries."
  else
    echo "[$(date '+%H:%M:%S')] Build failed. Waiting for next change..." >&2
  fi
}

if command -v fswatch >/dev/null 2>&1; then
  echo "Using fswatch backend."
  run_build
  fswatch -o "${WATCH_PATHS[@]}" | while read -r _; do
    run_build
  done
else
  echo "fswatch not found; using 2s polling backend. Install fswatch for lower latency."

  state=""
  while true; do
    new_state="$({
      find "$ROOT_DIR/src" -type f -print0
      find "$ROOT_DIR/data" -type f -print0
      find "$ROOT_DIR/cmake" -type f -print0
      printf '%s\0' "$ROOT_DIR/CMakeLists.txt" "$ROOT_DIR/CMakePresets.json" "$ROOT_DIR/buildspec.json"
    } | xargs -0 stat -f '%m %N' 2>/dev/null | shasum)"

    if [ "$new_state" != "$state" ]; then
      state="$new_state"
      run_build
    fi

    sleep 2
  done
fi
