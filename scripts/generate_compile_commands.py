#!/usr/bin/env python3
"""Generate a clean compile_commands.json from the Ninja build files.

Premake5's Ninja generator doesn't emit compile_commands.json, but Ninja
exposes `ninja -t compdb`. That output includes link edges, phony edges, and
multiple configurations. This script filters it down to a single, stable
Debug compile command per source file, which is what clangd needs for
jump-to-definition and diagnostics.

Run after regenerating build files:
    python3 scripts/generate_compile_commands.py
"""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE_EXTS = {".c", ".cc", ".cpp", ".cxx", ".mm"}


def is_compile_command(entry: dict) -> bool:
    cmd = entry.get("command", "")
    file = entry.get("file", "")
    if not cmd or not file:
        return False
    if " -c " not in cmd:
        return False
    if Path(file).suffix.lower() not in SOURCE_EXTS:
        return False
    return True


def prefer_debug_then_first(entries: list[dict]) -> list[dict]:
    """Keep one entry per source file, preferring a Debug command.

    Ninja's compdb lists multiple configurations (Debug/Release, sandbox/tests)
    for the same source. We keep the first Debug entry we see; if none exists,
    we fall back to the first entry. The resulting order matches Ninja's.
    """
    by_file: dict[str, dict] = {}
    for e in entries:
        f = e["file"]
        existing = by_file.get(f)
        if existing is None:
            by_file[f] = e
            continue
        # Prefer Debug over Release/unknown.
        if " -g " in e["command"] and " -g " not in existing["command"]:
            by_file[f] = e
    return list(by_file.values())


def main() -> int:
    try:
        raw = subprocess.check_output(
            ["ninja", "-t", "compdb"],
            cwd=ROOT,
            text=True,
        )
    except FileNotFoundError:
        print("error: ninja not found on PATH", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as e:
        print(f"error: ninja -t compdb failed: {e}", file=sys.stderr)
        return 1

    try:
        entries = json.loads(raw)
    except json.JSONDecodeError as e:
        print(f"error: failed to parse compdb JSON: {e}", file=sys.stderr)
        return 1

    compiled = [e for e in entries if is_compile_command(e)]
    deduped = prefer_debug_then_first(compiled)

    out = ROOT / "compile_commands.json"
    out.write_text(json.dumps(deduped, indent=2) + "\n")
    print(f"Generated {out} with {len(deduped)} entries")
    return 0


if __name__ == "__main__":
    sys.exit(main())
