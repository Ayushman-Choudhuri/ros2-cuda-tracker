#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
BUILD_TYPE="${1:-Debug}"

# Wipe stale cache if it was generated from a different source directory
CACHE_FILE="$BUILD_DIR/CMakeCache.txt"
if [[ -f "$CACHE_FILE" ]]; then
    CACHED_SRC=$(grep -m1 "^CMAKE_HOME_DIRECTORY:INTERNAL=" "$CACHE_FILE" | cut -d= -f2)
    if [[ "$CACHED_SRC" != "$PROJECT_ROOT" ]]; then
        echo "Stale cache (was: $CACHED_SRC). Wiping build dir."
        rm -rf "$BUILD_DIR"
    fi
fi

cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -sf "$BUILD_DIR/compile_commands.json" "$PROJECT_ROOT/compile_commands.json"
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo ""
echo "Executable: $BUILD_DIR/bin/tracker"
