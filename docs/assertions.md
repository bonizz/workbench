# Scene Assertions

Assertions let agents express expectations about scene state inside scripts. They are regular agent commands, so they work from both the Agent Console and `script.run`.

## Supported assertions

| Command | Description |
|---------|-------------|
| `assert.object_count <count>` | Total GameObjects in the scene, camera included. |
| `assert.object_exists <name>` | A GameObject with the given name exists. |
| `assert.selected [name]` | An object is selected; with a name, the selected object must match. |
| `assert.has_component <name> <component_type>` | The named object has the given component. |
| `assert.rotation <name> <x> <y> <z> [tolerance]` | The named object's Euler rotation (degrees) matches. |
| `assert.position <name> <x> <y> <z> [tolerance]` | The named object's position matches. |
| `assert.scale <name> <x> <y> <z> [tolerance]` | The named object's scale matches. |
| `assert.color <name> <r> <g> <b> <a> [tolerance]` | The named object's MeshRenderer color matches. |
| `assert.mesh <name> <shape>` | The named object's MeshRenderer shape is `cube`, `sphere`, or `plane`. |

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

## Numeric assertions

`assert.rotation`, `assert.position`, `assert.scale`, and `assert.color` compare
numeric values component-by-component. Each component must be within the tolerance:

```text
|actual - expected| <= tolerance
```

The default tolerance is `0.01`. An optional final argument overrides it:

```text
assert.position Cube 0 0.5 0
assert.position Cube 0 0.5 0 0.001
```

`assert.color` and `assert.mesh` require a `MeshRenderer`. If the object exists but has no
`MeshRenderer`, it is an assertion failure (not a usage error):

```text
Object 'Cube' has no MeshRenderer component.
```

A passing numeric assertion returns:

```text
OK
```

A failing one returns Expected / Actual text:

```text
Expected position: 0.0000, 0.0000, 0.0000
Actual position:   0.0000, 0.5000, 0.0000
```

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
- Numeric assertions use object names, not `ObjectIds`, so they are stable across loads.
- Component types are compared by `Component::typeName()`; only `MeshRenderer` is defined today.
