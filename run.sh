#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

SKIP_CLEAN=1 ./build.sh
./build/debug/sandbox
