# Agent Command Interface

Workbench exposes a small, text-based command surface that lets an agent (or a human developer) inspect and manipulate the editor without using the UI.

This is intentionally local and in-process. There is no networking, JSON, or MCP in this milestone. Those layers can be added later as thin wrappers around the same command functions.

## Philosophy

- One text command for every common editor action.
- No reflection, no command framework, no serialization.
- Plain struct context and plain struct result.
- The command layer is decoupled from the transport that might eventually call it.

## API

```cpp
struct AgentCommandContext {
    Scene& scene;
    GameObject*& selected;

    uint64_t frame = 0;
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    size_t renderCommandCount = 0;

    std::string lastScriptPath;
    MetalRenderer* renderer = nullptr;
    std::string lastCapturePath;
    std::string lastBundlePath;

    // Optional pointer to the owner's persistent last assertion failure.
    std::string* lastAssertionFailure = nullptr;
};

struct AgentCommandResult {
    bool success = false;
    std::string output;
};

AgentCommandResult executeCommand(const std::string& command, AgentCommandContext& ctx);
```

## Current Commands

| Command | Description |
|---|---|
| `agent.help [command]` | Show general help or help for a specific command. |
| `agent.commands` | List all available commands. |
| `scene.list` | List all objects sorted by id. |
| `scene.select <id>` | Select the object with the given id. |
| `scene.get_selected` | Show the current selection. |
| `scene.create_cube [name]` | Create a new GameObject at the origin. |
| `scene.delete <id>` | Delete the object with the given id. |
| `scene.duplicate <id>` | Duplicate the object with the given id. |
| `scene.save <filename>` | Save authored objects to `assets/scenes/<filename>`. |
| `scene.load <filename>` | Load authored objects from `assets/scenes/<filename>`. |
| `component.list <id>` | List components attached to a GameObject. |
| `component.add_mesh_renderer <id>` | Add a MeshRenderer component to a GameObject. |
| `component.add_rotator <name>` | Add a RotateComponent (angular velocity 0,0,0) to a named GameObject. |
| `component.set_rotator <name> <x> <y> <z>` | Set a RotateComponent's angular velocity in degrees per second. |
| `transform.get <id>` | Show position, rotation, and scale of an object. |
| `transform.set_position <id> <x> <y> <z>` | Set an object's position. |
| `debug.dump` | Return the same snapshot produced by `DebugState`. |
| `script.run <filename>` | Execute agent commands from `assets/scripts/<filename>`. |
| `render.capture [filename]` | Queue a viewport screenshot to `captures/<filename>`. |
| `debug.bundle <name>` | Create a repro bundle: `state.txt` + `screenshot.png`. |
| `sim.step [dt]` | Advance the simulation by `dt` seconds (default 1/60). Calls `Scene::update`. |
| `assert.object_count <count>` | Assert the total number of GameObjects. |
| `assert.object_exists <name>` | Assert a named GameObject exists. |
| `assert.selected [name]` | Assert an object is selected (optionally matching a name). |
| `assert.has_component <name> <type>` | Assert a GameObject has a component. |
| `assert.rotation <name> <x> <y> <z> [tolerance]` | Assert a named GameObject's Euler rotation in degrees (default tolerance 0.01). |
| `assert.position <name> <x> <y> <z> [tolerance]` | Assert a named GameObject's position (default tolerance 0.01). |
| `assert.scale <name> <x> <y> <z> [tolerance]` | Assert a named GameObject's scale (default tolerance 0.01). |
| `assert.color <name> <r> <g> <b> <a> [tolerance]` | Assert a named GameObject's MeshRenderer color rgba (default tolerance 0.01). |

## Command Discovery

An agent can discover the available command surface without reading documentation.

- `agent.help` shows how to use the interface and lists every command, including `agent.help` and `agent.commands`.
- `agent.commands` returns a concise list of command names.
- `agent.help <command>` returns usage, description, and an example for that command.

### Recommended agent startup sequence

1. `agent.help`
2. `agent.commands`
3. Query current state (`scene.list`, `debug.dump`, etc.)
4. Perform actions (`scene.select`, `transform.set_position`, etc.)

## Agent Console

The editor has an **Agent Console** panel where commands can be typed and executed. The result is printed in the panel.

This panel is for development and debugging. It is not meant to be the final agent integration point.

## Testing

Command behavior is covered by the existing `tests` executable:

- `agent.commands` returns all command names.
- `agent.help` returns the general help text.
- `agent.help scene.select` returns usage, description, and an example.
- Unknown help targets return a useful error.
- `scene.list` returns created objects.
- `scene.select` updates the selection pointer.
- `transform.set_position` updates object state.
- `debug.dump` returns a valid state snapshot.

## Simulation and determinism

In **interactive mode** the simulation advances every frame, so behavior components such as `RotateComponent` animate live in the editor.

In **automation mode** (`--run-script`, `--run-tests`, `--bundle`) the per-frame auto-advance is disabled. The simulation advances **only** when a script calls `sim.step [dt]`. This keeps scripts and tests deterministic: the same `sim.step` values always produce the same scene state. See `docs/components.md` for the exact update order.

## Future MCP Direction

A future MCP adapter can:

1. Receive a request string.
2. Build an `AgentCommandContext`.
3. Call `executeCommand`.
4. Wrap `AgentCommandResult::output` into the response format the protocol expects.

The command logic itself should not need to change.
