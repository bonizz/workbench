# Components

Workbench uses a traditional **GameObject + Component** model. Components are no longer inert data: they have a minimal **lifecycle** (`onStart` / `onUpdate`) and can drive runtime behavior. The first behavior component is `RotateComponent`; `MeshRenderer` remains the only rendering component.

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

    // Lifecycle hooks. Default no-ops.
    virtual void onStart() {}
    virtual void onUpdate(float /*deltaTime*/) {}

    // Drives the lazy one-shot onStart transition.
    void startIfNeeded();

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
4. (If it has behavior) nothing else — `onStart`/`onUpdate` are picked up by `Scene::update` automatically.

This is intentional. Complexity is added only when a real problem justifies it.

## Ownership

`GameObject` owns its components via `std::vector<std::unique_ptr<Component>>`. Components can be accessed with typed helpers:

```cpp
if (MeshRenderer* mesh = obj->getComponent<MeshRenderer>()) {
    // ...
}
```

`dynamic_cast` is used because the type set is tiny and explicit. If the type set grows, this can be replaced with a tag or variant later.

## Component Lifecycle

`Component` has two lifecycle hooks, both default no-ops so existing components are unaffected:

- `onStart()` — called once, before the component's first `onUpdate`.
- `onUpdate(float dt)` — called every simulation step thereafter.

The lifecycle is driven by `Scene::update(float dt)`. The start transition is encapsulated in `Component::startIfNeeded()`, which calls `onStart()` the first time it is invoked and is a no-op afterwards. Callers never touch private started state.

### Update order (deterministic)

`Scene::update(dt)` iterates in this exact order:

1. **Objects** in creation order, which is `ObjectId` ascending, which is the `objects_` vector order.
2. **Components per object** in add order, which is the `components_` vector order.
3. For each component: `onStart()` once (lazily, immediately before its first `onUpdate`), then `onUpdate(dt)`.

Inactive objects (`active == false`) are skipped entirely — no `onStart`, no `onUpdate`.

`Camera` has no components, so `Scene::update` never touches it. `Camera::update` continues to be called separately by `Application`.

### When the simulation advances

- **Interactive mode** (`./build/debug/sandbox`): `Application` calls `Scene::update(realFrameDt)` every frame, so behavior components animate live in the editor.
- **Automation mode** (`--run-script`, `--run-tests`, `--bundle`): the simulation advances **only** via the `sim.step [dt]` agent command. The per-frame auto-advance is disabled so scripts and tests are deterministic and not polluted by real-time frame deltas.

### Cloned and loaded components

`Component::clone()` and deserialization produce fresh component instances with `started_ == false`. A duplicated or loaded behavior component begins its own lifecycle on the next `Scene::update`. This is the desired, least-surprising behavior.

## MeshRenderer

`MeshRenderer` is currently the only way to make an object visible. The renderer iterates objects and draws a cube for each object that has a `MeshRenderer` component.

The `mesh` field is a placeholder string. Only `"cube"` is supported today.

## RotateComponent

`RotateComponent` is a behavior component that rotates its owning GameObject.

```cpp
class RotateComponent : public Component {
public:
    const char* typeName() const override { return "RotateComponent"; }
    void onUpdate(float dt) override;
    Vec3 angularVelocityEuler{0.0f, 0.0f, 0.0f}; // degrees per second
};
```

`onUpdate(dt)` adds `angularVelocityEuler * dt` to `Transform::rotation`.

### Units (important)

- `Transform::rotation` is stored in **radians** (see `docs/scenes.md`, `DebugState`).
- `RotateComponent::angularVelocityEuler` is in **degrees per second**.
- `onUpdate` converts deg/s to rad/s internally, scales by `dt`, and adds to the radian rotation.
- `assert.rotation` and `DebugState` report rotation in **degrees**.

This keeps the author-facing surface (commands, serialization, debug) in intuitive degrees while the engine's internal transform stays in radians.

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
        },
        {
          "type": "RotateComponent",
          "angularVelocity": [0.0000, 90.0000, 0.0000]
        }
      ]
    }
  ]
}
```

Objects without a `MeshRenderer` will load but will not render. `scene.load` returns a warning in that case.

## Duplication

When an object is duplicated, each component is cloned via `Component::clone()`. This preserves `MeshRenderer` color and mesh settings, and `RotateComponent` angular velocity, on the copy. Cloned components start a fresh lifecycle on the next update.

## Agent Commands

- `component.list <id>` — list components on an object (id-based, legacy)
- `component.add_mesh_renderer <id>` — add a `MeshRenderer` to an object (id-based, legacy)
- `component.add_rotator <name>` — add a `RotateComponent` (velocity 0,0,0) to a named object (name-based)
- `component.set_rotator <name> <x> <y> <z>` — set a `RotateComponent`'s angular velocity in deg/s (name-based)
- `sim.step [dt]` — advance the simulation by `dt` seconds (default 1/60); calls `Scene::update`
- `assert.rotation <name> <x> <y> <z> [tolerance]` — assert a named object's Euler rotation in degrees (default tolerance 0.01)

The newer rotator/simulation commands are **name-based** so that `.wbs` tests do not depend on runtime `ObjectIds`, which are reassigned on load and differ across runs. The original `component.add_mesh_renderer` remains id-based and unchanged. This small inconsistency is intentional for now and will be reconciled when a future milestone revisits the command surface.

## Future Direction

Later milestones may add:

- `Camera` as a component
- Additional renderer features (wireframe, materials, etc.)
- Component removal commands
- More component types as the engine grows

The current model is intentionally minimal so those additions can be made without undoing a heavy abstraction.
