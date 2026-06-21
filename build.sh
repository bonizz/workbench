#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# Header dependency tracking now works (see the ninja override in premake5.lua),
# so incremental builds are reliable -- run.sh uses SKIP_CLEAN=1 for that. A clean
# is still the default here so a plain ./build.sh produces a pristine full build.
if [[ "${SKIP_CLEAN:-0}" != "1" ]]; then
    echo "Cleaning previous build artifacts..."
    rm -rf build/obj build/debug build/release build/tests
fi

echo "Generating build files..."
premake5 ninja

echo "Building debug..."
ninja Debug

echo "Done. Run with: ./build/debug/sandbox"
