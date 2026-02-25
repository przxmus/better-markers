#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build_macos"
CONFIG="RelWithDebInfo"
RUNDIR="$(pwd)/${BUILD_DIR}/rundir/${CONFIG}"
OBS_APP_PATH="/Applications/OBS.app"

if [ ! -d "$OBS_APP_PATH" ]; then
  echo "OBS not found at $OBS_APP_PATH" >&2
  exit 1
fi

if [ ! -d "$RUNDIR" ]; then
  echo "Missing $RUNDIR. Build first with ./scripts/dev/build-install.sh or cmake --build." >&2
  exit 1
fi

# Launch as a normal app bundle and pass env through launchd to avoid
# background-only behavior from direct binary execution.
launchctl setenv OBS_PLUGINS_PATH "$RUNDIR"
launchctl setenv OBS_PLUGINS_DATA_PATH "$RUNDIR"

open -a OBS

# Give launch enough time to inherit env vars, then clean up global state.
sleep 3
launchctl unsetenv OBS_PLUGINS_PATH || true
launchctl unsetenv OBS_PLUGINS_DATA_PATH || true
