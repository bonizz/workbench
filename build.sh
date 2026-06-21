#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# Header dependency tracking now works (see the ninja override in premake5.lua),
# so incremental builds are reliable. Use ./clean.sh for a pristine full build.
#
# Quiet by default: agents (and anyone piping output) get a single status line
# on success and the failing log tail on error, which keeps build output cheap.
# Run in an interactive terminal, or pass VERBOSE=1, to see full progress.
if [ "${VERBOSE:-}" = "1" ] || [ -t 1 ]; then
    echo "Generating build files..."
    premake5 ninja
    echo "Building debug..."
    ninja Debug
    echo "Done. Run with: ./build/debug/sandbox"
else
    log=$(mktemp)
    if ! { premake5 ninja && ninja Debug; } > "$log" 2>&1; then
        tail -50 "$log"
        rm -f "$log"
        exit 1
    fi
    rm -f "$log"
    echo "build OK. Run with: ./build/debug/sandbox"
fi
