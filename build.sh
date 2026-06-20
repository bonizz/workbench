#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "Generating build files..."
premake5 ninja

echo "Building debug..."
ninja Debug

echo "Done. Run with: ./build/debug/sandbox"
