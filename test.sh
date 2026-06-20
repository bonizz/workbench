#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "Generating build files..."
premake5 ninja

echo "Building tests..."
ninja build/tests/debug/tests

echo "Running tests..."
./build/tests/debug/tests
