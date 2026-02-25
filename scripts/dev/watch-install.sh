#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_SCRIPT="$ROOT_DIR/scripts/dev/build-install.sh"
WATCH_PATHS=("$ROOT_DIR/src" "$ROOT_DIR/data" "$ROOT_DIR/CMakeLists.txt" "$ROOT_DIR/buildspec.json")

run_build() {
  echo "[$(date '+%H:%M:%S')] Rebuilding and installing..."
  if "$BUILD_SCRIPT"; then
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
      printf '%s\0' "$ROOT_DIR/CMakeLists.txt" "$ROOT_DIR/buildspec.json"
    } | xargs -0 stat -f '%m %N' 2>/dev/null | shasum)"

    if [ "$new_state" != "$state" ]; then
      state="$new_state"
      run_build
    fi

    sleep 2
  done
fi
