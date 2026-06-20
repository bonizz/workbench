# Scene Assertions

Assertions let agents express expectations about scene state inside scripts. They are regular agent commands, so they work from both the Agent Console and `script.run`.

## Supported assertions

| Command | Description |
|---------|-------------|
| `assert.object_count <count>` | Total GameObjects in the scene, camera included. |
| `assert.object_exists <name>` | A GameObject with the given name exists. |
| `assert.selected [name]` | An object is selected; with a name, the selected object must match. |
| `assert.has_component <name> <component_type>` | The named object has the given component. |

## Success and failure

A passing assertion returns:

```text
OK
```

A failing assertion returns a message such as:

```text
Expected object count: 2
Actual object count: 1
```

or

```text
Object not found: CubeA
```

Assertion failures set `Last Assertion Failure` in `DebugState` so it persists across commands and frames.

## Script as integration test

A script can describe a scene mutation and then verify the result:

```text
scene.create_cube CubeA
assert.object_exists CubeA
assert.has_component CubeA MeshRenderer
assert.object_count 3
```

The default scene already contains the camera and a cube, so after creating `CubeA` the total object count is 3.

## CLI workflow

Create a script in `assets/scripts/create_cube_test.wbs`:

```text
scene.create_cube TestCube
assert.object_exists TestCube
assert.has_component TestCube MeshRenderer
```

Run it unattended:

```bash
./build/debug/sandbox --run-script create_cube_test.wbs --exit
```

- If every assertion passes: exit code `0`.
- If any assertion fails: exit code `1`, and the failure is printed to stderr.

## Example failure

Script:

```text
assert.object_exists MissingCube
```

Output:

```text
FAIL:
Object not found: MissingCube
```

## Limitations

- No boolean logic, variables, loops, or JSON expressions.
- `assert.object_exists` matches the first object with the given name.
- Component types are compared by `Component::typeName()`; only `MeshRenderer` is defined today.
