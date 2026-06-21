#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# Header dependency tracking now works (see the ninja override in premake5.lua),
# so incremental builds are reliable. Use ./clean.sh for a pristine full build.

echo "Generating build files..."
premake5 ninja

echo "Building debug..."
ninja Debug

echo "Done. Run with: ./build/debug/sandbox"
