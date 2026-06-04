#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="$(dirname "$0")/build"
BUILD_TYPE="${1:-Debug}"

# Wipe stale cache if it was generated from a different source directory
CACHE_FILE="$BUILD_DIR/CMakeCache.txt"
if [[ -f "$CACHE_FILE" ]]; then
    CACHED_SRC=$(grep -m1 "^CMAKE_HOME_DIRECTORY:INTERNAL=" "$CACHE_FILE" | cut -d= -f2)
    ABS_SRC="$(cd "$(dirname "$0")" && pwd)"
    if [[ "$CACHED_SRC" != "$ABS_SRC" ]]; then
        echo "Stale cache (was: $CACHED_SRC). Wiping build dir."
        rm -rf "$BUILD_DIR"
    fi
fi

cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -sf "$BUILD_DIR/compile_commands.json" "$(dirname "$0")/compile_commands.json"
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo ""
echo "Executable: $BUILD_DIR/bin/Camera"
