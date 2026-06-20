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

## Transform Hierarchy

GameObjects can be arranged in a parent/child tree. The `Transform` fields
(`position`, `rotation`, `scale`) are always **local** to an object's parent;
the **world** transform is derived as `parent.worldMatrix * localMatrix`.

### Parenting

From the Agent Console or a script (name-based, stable across load):

```
scene.create_cube Parent
scene.create_cube Child
scene.set_parent Child Parent
scene.detach Child
scene.get_parent Child
scene.get_children Parent
```

Rules:

- A root object's local transform **is** its world transform.
- A child's world transform composes its parent's world with its own local.
- The camera cannot be a parent or a child.
- `set_parent` rejects cycles, self-parenting, and the camera.
- `scene.delete` and `scene.duplicate` **cascade** the whole subtree.
- `transform.set_position` and the inspector edit **local** fields. To verify
  composed placement, use `assert.world_position <name> <x> <y> <z> [tol]`.

### Serialized form

Hierarchy is stored as a nested `children` array. Objects with no children
serialize exactly as before (the field is omitted), so flat scenes stay
byte-for-byte identical and load unchanged.

```json
{
  "objects": [
    {
      "name": "Parent",
      "position": [0.0000, 0.0000, 0.0000],
      "rotation": [0.0000, 0.0000, 0.0000],
      "scale": [1.0000, 1.0000, 1.0000],
      "components": [ {"type": "MeshRenderer", "mesh": "cube", "color": [1.0, 1.0, 1.0, 1.0]} ],
      "children": [
        {
          "name": "Child",
          "position": [1.0000, 0.0000, 0.0000],
          "rotation": [0.0000, 0.0000, 0.0000],
          "scale": [0.5000, 0.5000, 0.5000],
          "components": []
        }
      ]
    }
  ]
}
```

Only local fields are serialized; world matrices are derived on load. Children
are written in attachment (insertion) order.

### Editor

The Hierarchy panel renders the scene as an indented tree. Drag an object onto
another to reparent it (invalid drops — onto the camera or a descendant — are
silently rejected by `Scene`). Drop onto the empty area, or use the **Detach**
button, to make an object a root. The Inspector shows the selected object's
parent and live world position as read-only fields.

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
