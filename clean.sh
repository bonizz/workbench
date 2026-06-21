#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "Cleaning build artifacts..."
rm -rf build/obj build/debug build/release build/tests
