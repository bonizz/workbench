# Agent Observability

Workbench exposes a small, deterministic text snapshot of engine state so an AI agent can inspect a bug without relying on screenshots.

## DebugState

`src/engine/debug/debug_state.h` provides:

```cpp
namespace DebugState {
    std::string build(uint64_t frame,
                      float fps,
                      float frameTimeMs,
                      size_t renderCommandCount,
                      const Scene& scene,
                      const GameObject* selected);

    void writeToFile(const std::string& text,
                     const std::string& path = "logs/latest_state.txt");
}
```

The dump includes:

- Current frame, FPS, and frame time
- Every object in the scene, sorted by `ObjectId`, with name, position, rotation (degrees), and scale
- The editor's current selection
- The most recent render command count

The format is plain text, stable, and easy to paste into a chat context.

## Diagnostic workflow

When something looks wrong:

1. Click **Write Debug State** to save `logs/latest_state.txt`.
2. Or click **Copy Debug State** to copy the snapshot to the clipboard.
3. Paste the text into the agent chat.

The agent can reason from actual engine state instead of inferring it from visuals.

## Notes for agents

- Object ids are allocated sequentially by `Scene` and are stable for the lifetime of the object.
- Scene objects are sorted by id in the dump, so the order is deterministic.
- `renderCommands` is the count produced by the last completed render frame.
- `selected: none` means no object is selected in the editor.
