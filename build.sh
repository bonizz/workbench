#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# premake5's ninja action does not reliably track header dependencies yet.
# Clean builds avoid stale object files causing layout/ODR bugs.
# We can revisit incremental builds once dependency tracking is solid.
if [[ "${SKIP_CLEAN:-0}" != "1" ]]; then
    echo "Cleaning previous build artifacts..."
    rm -rf build/obj build/debug build/release
fi

echo "Generating build files..."
premake5 ninja

echo "Building debug..."
ninja Debug

echo "Done. Run with: ./build/debug/sandbox"
