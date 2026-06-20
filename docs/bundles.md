# Repro Bundles

A repro bundle collects the two most useful diagnostic artifacts for bug reports and code reviews into a single directory: the current engine state and a screenshot of the viewport.

## Creating a bundle

Agent command:

```text
debug.bundle <name>
```

Example:

```text
debug.bundle cube_overlap
```

This creates:

```text
bundles/cube_overlap/
├── state.txt
└── screenshot.png
```

- `state.txt` is the same output as `debug.dump`. It is written synchronously before the command returns.
- `screenshot.png` is queued for asynchronous capture from the next frame's drawable (ImGui included) and appears shortly after the frame completes.

The command output explicitly reports which file is synchronous and which is queued:

```text
Created bundle:
bundles/cube_overlap/

Files:
  state.txt      (written synchronously)
  screenshot.png (queued asynchronously)
```

## Bundle names

Names must be non-empty and cannot contain path separators (`/` or `\`) or traversal components (`.` or `..`). The `bundles/` directory is created automatically.

## Intended agent workflow

```text
script.run repro_case_001.wbs
debug.bundle repro_case_001
```

Result:

```text
bundles/repro_case_001/
├── state.txt
└── screenshot.png
```

This directory is sufficient to attach to a bug report or review:

- `state.txt` shows the exact scene state, selection, and frame statistics.
- `screenshot.png` shows what the user/agent saw.

## Editor panel

The editor has a **Repro Bundle** panel where a bundle name can be entered and a **Create Bundle** button queues the bundle.

## DebugState

After a bundle is created, `debug.dump` reports:

```text
Last Bundle: bundles/<name>/
```
