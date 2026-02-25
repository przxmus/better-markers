#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build_macos"
CONFIG="RelWithDebInfo"
RUNDIR="$(pwd)/${BUILD_DIR}/rundir/${CONFIG}"
OBS_APP="/Applications/OBS.app/Contents/MacOS/OBS"

if [ ! -x "$OBS_APP" ]; then
  echo "OBS not found at $OBS_APP" >&2
  exit 1
fi

if [ ! -d "$RUNDIR" ]; then
  echo "Missing $RUNDIR. Build first with ./scripts/dev/build-install.sh or cmake --build." >&2
  exit 1
fi

export OBS_PLUGINS_PATH="$RUNDIR"
export OBS_PLUGINS_DATA_PATH="$RUNDIR"

exec "$OBS_APP"
