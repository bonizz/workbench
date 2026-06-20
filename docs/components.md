# Components

Workbench uses a traditional **GameObject + Component** model. This milestone introduces the smallest possible component system with a single concrete component: `MeshRenderer`.

## Design

A `GameObject` is a container:

- `Transform`
- list of owned `Component` instances

```cpp
class Component {
public:
    virtual ~Component() = default;
    virtual const char* typeName() const = 0;
    virtual std::unique_ptr<Component> clone() const = 0;
    GameObject* gameObject() const;
};
```

```cpp
class MeshRenderer : public Component {
public:
    std::string mesh = "cube";
    simd::float4 color = {1, 1, 1, 1};
};
```

There are no factories, registries, reflection, or dynamic registration. Adding a new component type requires:

1. Creating the component class.
2. Updating the serializer to read/write it.
3. Updating the editor inspector to edit it.

This is intentional. Complexity is added only when a real problem justifies it.

## Ownership

`GameObject` owns its components via `std::vector<std::unique_ptr<Component>>`. Components can be accessed with typed helpers:

```cpp
if (MeshRenderer* mesh = obj->getComponent<MeshRenderer>()) {
    // ...
}
```

`dynamic_cast` is used because the type set is tiny and explicit. If the type set grows, this can be replaced with a tag or variant later.

## MeshRenderer

`MeshRenderer` is currently the only way to make an object visible. The renderer iterates objects and draws a cube for each object that has a `MeshRenderer` component.

The `mesh` field is a placeholder string. Only `"cube"` is supported today.

## Camera

The camera is still a special `GameObject` subclass (`Camera`) and is **not** a component yet. It will be migrated in a future milestone once the component model is proven.

## Serialization

Scene files store components per object:

```json
{
  "objects": [
    {
      "name": "Cube",
      "position": [0.0000, 0.5000, 0.0000],
      "rotation": [0.0000, 0.0000, 0.0000],
      "scale": [0.5000, 0.5000, 0.5000],
      "components": [
        {
          "type": "MeshRenderer",
          "mesh": "cube",
          "color": [0.9500, 0.5500, 0.2000, 1.0000]
        }
      ]
    }
  ]
}
```

Objects without a `MeshRenderer` will load but will not render. `scene.load` returns a warning in that case.

## Duplication

When an object is duplicated, each component is cloned via `Component::clone()`. This preserves `MeshRenderer` color and mesh settings on the copy.

## Agent Commands

- `component.list <id>` — list components on an object
- `component.add_mesh_renderer <id>` — add a `MeshRenderer` to an object

## Future Direction

Later milestones may add:

- `Camera` as a component
- Additional renderer features (wireframe, materials, etc.)
- Component removal commands
- More component types as the engine grows

The current model is intentionally minimal so those additions can be made without undoing a heavy abstraction.
