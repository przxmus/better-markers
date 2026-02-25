#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build_macos"
CONFIG="RelWithDebInfo"

if [ ! -d "$BUILD_DIR" ]; then
  echo "Missing $BUILD_DIR. Run ./scripts/dev/configure.sh first." >&2
  exit 1
fi

cmake --build "$BUILD_DIR" --config "$CONFIG"
cmake --install "$BUILD_DIR" --config "$CONFIG"

echo "Installed plugin to ~/Library/Application Support/obs-studio/plugins"
