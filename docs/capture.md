# Capture

Workbench can capture the current viewport to a PNG file. The capture includes everything visible in the window, including the ImGui UI.

## Usage

Agent command:

```text
render.capture [filename]
```

Examples:

```text
render.capture
render.capture test_scene.png
```

- If no filename is given, Workbench generates `capture_YYYYMMDD_HHMMSS.png`.
- Files are written to `captures/`.
- The directory is created automatically.

## Implementation notes

The renderer already draws the scene to an offscreen texture, blits it to the window drawable, and then renders ImGui on top. `render.capture` queues a read-back of that final drawable and writes it asynchronously to a PNG. No additional render pass is introduced.

Because the PNG write happens on the command buffer completion handler, `render.capture` reports:

```text
Queued screenshot: captures/test_scene.png
```

The file is written shortly after the current frame completes.

## DebugState

After a capture is queued, `debug.dump` includes:

```text
Last Capture: captures/test_scene.png
```

## Editor panel

The editor has a **Screenshot** panel where a filename can be entered and a **Capture Screenshot** button queues the capture.

## Intended agent workflow

A reproducible visual bug report can be produced with three commands:

```text
script.run repro_case_001.wbs
render.capture repro_case_001.png
debug.dump
```

This yields:

- The reproduction script
- A viewport screenshot
- A full engine state snapshot

All three can be attached to a bug report or review request.
