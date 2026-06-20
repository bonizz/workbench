# CLI Automation

Workbench can run automation from the command line without human interaction. This is useful for agent self-testing and generating repro bundles from scripts.

## Flags

| Flag | Argument | Description |
|------|----------|-------------|
| `--run-script` | `<filename>` | Runs `assets/scripts/<filename>` through the existing Script Runner. |
| `--bundle` | `<name>` | Creates a repro bundle at `bundles/<name>/` using the existing `debug.bundle` command. |
| `--exit` | none | Terminates the app after automation finishes. |
| `--frames` | `<N>` | Waits N extra frames before exiting (default 3). This gives the asynchronous screenshot time to write. |

## Example

```bash
./build/debug/sandbox --run-script create_test_scene.wbs --bundle cli_smoke --exit
```

Expected artifacts:

```text
bundles/cli_smoke/
├── state.txt
└── screenshot.png
```

## How it works

1. `src/sandbox/main.mm` parses the command line into a `CliOptions` struct.
2. The options are passed to `Application`.
3. On the first frame, `Application::onUpdate` runs automation through `executeCommand`:
   - `--run-script` executes `script.run <filename>`.
   - `--bundle` executes `debug.bundle <name>`.
4. If `--exit` is set, the app renders `--frames` more frames, then calls `Window::terminate()`.
5. The extra frames allow the Metal command buffer completion handler to write `screenshot.png` asynchronously.

## Agent workflow

A typical agent self-test looks like:

```bash
./build/debug/sandbox --run-script repro_case.wbs --bundle repro_case --exit
```

The agent can then read `bundles/repro_case/state.txt` and inspect `bundles/repro_case/screenshot.png` without launching the UI.

## Limitations

- CLI automation runs only after the renderer is initialized, on the first frame.
- `--frames` is a simple frame count, not a timeout. The default of 3 is enough for the current capture path on modern hardware.
- There is no headless rendering; the app still opens a window and uses Metal normally.
