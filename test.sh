#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# Quiet by default: agents (and anyone piping output) get a single status line
# on success and the failing log tail on error, which keeps build output cheap.
# Run in an interactive terminal, or pass VERBOSE=1, to see full progress.
if [ "${VERBOSE:-}" = "1" ] || [ -t 1 ]; then
    echo "Generating build files..."
    premake5 ninja
    echo "Building tests..."
    ninja build/tests/debug/tests
    echo "Running tests..."
    ./build/tests/debug/tests
else
    log=$(mktemp)
    if ! { premake5 ninja \
            && ninja build/tests/debug/tests \
            && ./build/tests/debug/tests; } > "$log" 2>&1; then
        tail -50 "$log"
        rm -f "$log"
        exit 1
    fi
    rm -f "$log"
    echo "tests OK"
fi
