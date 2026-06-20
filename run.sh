#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# run.sh does an incremental build so startup stays fast during iteration.
# Use ./build.sh directly for a clean build.
SKIP_CLEAN=1 ./build.sh
./build/debug/sandbox
