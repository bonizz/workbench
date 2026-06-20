# Scripts

Workbench has a minimal **Script Runner** that executes existing agent commands from a text file. It is not a scripting language—there are no variables, conditionals, loops, or expressions.

## Format

Scripts are plain text files stored in `assets/scripts/`. The conventional extension is `.wbs`.

Rules:

- One command per line.
- Blank lines are ignored.
- Lines starting with `#` are comments and ignored.
- Commands are exactly the same strings accepted by the Agent Console.

Example (`assets/scripts/create_test_scene.wbs`):

```text
# Create two cubes and save the scene.

scene.create_cube CubeA
transform.set_position 2 1.0 0.0 0.0

scene.create_cube CubeB
transform.set_position 3 -1.0 0.0 0.0

scene.save sample.scene
```

## Execution

Run a script with the agent command:

```text
script.run <filename>
```

The filename is resolved relative to `assets/scripts/`.

Execution behavior:

- Commands run sequentially in file order.
- Execution stops on the first failing command.
- The result reports how many commands executed and prints the transcript.

Example success:

```text
Executed 3 commands.

scene.create_cube CubeA
-> Created CubeA [2]

scene.save sample.scene
-> Saved scene to assets/scenes/sample.scene
```

Example failure:

```text
Error on line 2 (transform.set_position 99 0 0 0): Object not found: 99
```

## Editor Panel

The editor has a **Script Runner** panel where a script filename can be typed and run. Output appears in the same panel.

## DebugState

After a script runs successfully, `debug.dump` includes:

```text
Last Script: create_test_scene.wbs
```

This makes it easy to correlate engine state with the script that produced it.

## Intended Agent Workflow

1. Agent creates a reproducible case as `assets/scripts/repro_case_001.wbs`.
2. Agent runs `script.run repro_case_001.wbs`.
3. Agent captures `debug.dump` to record the resulting state.

This produces self-contained bug reports and regression tests without adding a scripting runtime.
