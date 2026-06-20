# Scene Authoring

Workbench scenes are simple collections of GameObjects. This milestone intentionally avoids components, prefabs, asset pipelines, and editor-state serialization.

## Scene File Format

Scenes are stored as human-readable text files under `assets/scenes/`.

```json
{
  "objects": [
    {
      "name": "Cube",
      "position": [0.0000, 0.5000, 0.0000],
      "rotation": [0.0000, 0.0000, 0.0000],
      "scale": [0.5000, 0.5000, 0.5000],
      "color": [0.9500, 0.5500, 0.2000, 1.0000]
    }
  ]
}
```

Notes:

- `ObjectId` values are **not** serialized. They are runtime/editor handles.
- The camera is not serialized; it is preserved across load.
- `rotation` is stored in radians.
- Numbers are written with 4 decimal places so scene files diff well in git.

## Save / Load Workflow

From the **Agent Console** or from code:

```
scene.save test.scene
scene.load test.scene
```

Paths are resolved relative to `assets/scenes/`. `save` creates the directory if needed.

`load` replaces all authored objects (everything except the camera) and clears the current selection. After loading, use `scene.list` to discover the new runtime ids.

## UI Workflow

The **Hierarchy** panel has three buttons:

- **Create Cube** — adds a new white cube at the origin and selects it.
- **Duplicate** — duplicates the selected object and selects the copy.
- **Delete** — removes the selected object and clears selection.

The camera cannot be duplicated or deleted.

## Agent Workflow Example

```
scene.create_cube TestA
transform.set_position 2 1.0 0.0 0.0

scene.create_cube TestB
transform.set_position 3 -1.0 0.0 0.0

scene.save test.scene
```

Later, in a fresh session:

```
scene.load test.scene
scene.list
debug.dump
```

Because ids are reassigned on load, always call `scene.list` before referring to objects by id.

## Future Direction

Later milestones may add:

- Prefabs / templates
- Multiple component types
- Binary serialization
- Scene references / sub-scenes

The current format is intentionally minimal so those additions can be layered on without carrying legacy complexity.
