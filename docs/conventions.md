# Workbench Conventions

READ THIS FIRST. These are the contracts and conventions that source files
alone do not surface. Everything here was learned the hard way.

## 1. Determinism model

The simulation must advance identically across runs in automation.

- **Interactive mode** (`./build/debug/sandbox`): `Application::onUpdate` calls
  `Scene::update(realFrameDt)` every frame, so behavior components animate live.
- **Automation mode** (`--run-script`, `--bundle`, `--run-tests`): the per-frame
  auto-advance is **disabled**. The simulation advances **only** via the
  `sim.step [dt]` agent command.

The mode is decided once in `Application::init`:

```cpp
liveSimulation_ = cliOptions_.runScript.empty()
               && cliOptions_.bundleName.empty()
               && !cliOptions_.runTests;
```

`Application` also runs an automation state machine (`Pending → RanCommands →
WaitingExtraFrames → Done`) in `core/application.cpp`. Tests must never depend on
real-time frame deltas.

## 2. sim.step and automation mode

- `sim.step [dt]` calls `Scene::update(dt)`. Default `dt` is `1/60`; tests always
  pass an explicit value (e.g. `1.0`) for determinism.
- `Scene::update(dt)` iterates objects in creation/`ObjectId` order, skipping
  inactive ones, and per object iterates components in add order. Each component
  gets `onStart()` once (lazily, before its first `onUpdate`) then `onUpdate(dt)`.
- `Camera` is updated separately by `Application` (`Camera::update`), every frame,
  in all modes. It is unaffected by `Scene::update`.

If you add behavior that should be testable, route it through `Scene::update` and
exercise it with `sim.step` + an assertion. Never add a per-frame update that
runs in automation mode.

## 3. Camera special rules

The camera is a `GameObject` subclass (`Camera`) but is treated specially
everywhere:

- Created via `Scene::createCamera` and stored in `objects_` like any object.
  `Scene::isCamera(obj)` identifies it.
- Marked **inactive** (`setActive(false)`) by `Application` on creation, so it is
  skipped by `Scene::update` and `buildRenderCommands`.
- Updated through its own `Camera::update(dt, input)` every frame by
  `Application`, not through `Scene::update`.
- **Never serialized.** `SceneSerializer::save` skips `isCamera` objects.
- **Cannot be deleted or duplicated.** `Scene::deleteObject` /
  `duplicateObject` reject the camera.
- First object in a fresh scene: its `ObjectId` is `1`; the first created object
  is `2`.

## 4. Math, rotation, and units

- `Transform::rotation` is stored in **radians** as Euler angles.
- `Transform::localMatrix()` is built as `T * Ry * Rx * Rz * S`
  (translate ∘ Yaw ∘ Pitch ∘ Roll ∘ Scale). Read `scene/transform.cpp` and
  `core/math.cpp` before assuming a rotation direction.
- Author-facing surfaces are in **degrees**: `RotateComponent::angularVelocityEuler`
  (deg/s), every `assert.rotation`, and the DebugState rotation display.
- Shared constants live in `core/math.h`: `kPi`, `kDegToRad`, `kRadToDeg`.
  Note: `agent/command.cpp` and `debug/debug_state.cpp` each also define a
  private `kRadToDeg`. Prefer the shared one in new code; do not refactor the
  duplicates unless that is the task.
- Renderer uses reversed-Z perspective (`core/math.cpp`).
- **Transform hierarchy (local vs world).** `Transform` fields are **local** to
  the object's parent; the **world** matrix is a derived cache refreshed by
  `Scene::updateWorldTransforms()` (DFS from roots). Authoring commands
  (`transform.set_position`, the inspector) edit local fields. Rendering uses
  `worldMatrix()`. `assert.position` checks local; `assert.world_position`
  checks world. The camera cannot be a parent or child. Delete/duplicate
  cascade the whole subtree. See `docs/scenes.md`.

When in doubt about a rotation's effect, derive it from `math.cpp`'s matrix
functions, or write a tiny standalone check.

### Matrix / SIMD conventions

- Workbench uses `simd::float4x4` / `simd::float3x3`.
- simd matrices are column-indexed in code: `m.columns[i]`.
- That does not mean every basis vector is stored in a column.
- Our `lookAt()` view matrix stores camera basis vectors as the rows of the
  upper-left 3×3:
  - `right    = {m.columns[0].x, m.columns[1].x, m.columns[2].x}`
  - `up       = {m.columns[0].y, m.columns[1].y, m.columns[2].y}`
  - `backward = {m.columns[0].z, m.columns[1].z, m.columns[2].z}`
- Do not assume `columns[0].xyz` is world-space right.
- Add non-identity camera tests for camera/ray/math changes.

### Directional light convention

`LightSettings.direction` is the direction the light **travels** (e.g. `{0,-1,0}`
is a sun directly overhead shining straight down). Two derived vectors follow from
it, and both are the **negation**:

- **Surface-to-light vector** (for Lambert `dot(n, l)`): `l = normalize(-direction)`.
- **Visual sun direction** (where the sun disk sits in the sky): `-direction`.

Both `assets/shaders/basic.metal` (mesh lighting) and `assets/shaders/sky.metal`
(sun disk) negate `direction`, so the lit face of an object always points at the
visible sun. **Pitfall:** an earlier build lit meshes with `+direction`, so the
*away* face was lit while the sun rendered on the opposite side. If lighting and
the sun ever disagree again, check the sign first.

## 5. id vs name addressing

Agent commands address objects two ways:

- **id-based (legacy):** `scene.select`, `scene.delete`, `scene.duplicate`,
  `transform.get`, `transform.set_position`, `component.list`,
  `component.add_mesh_renderer`. These take a runtime `ObjectId`.
- **name-based (new):** `component.add_rotator`, `component.set_rotator`, and all
  `assert.*` commands. These take an object name.

**`ObjectId`s are NOT serialized** and are reassigned on load. Therefore `.wbs`
tests and repro scripts must use **name-based** commands (or call `scene.list`
after load to discover fresh ids). New commands should be name-based.

`Scene::findObjectByName` returns the **first** match. Scripts must use unique
names to keep lookups deterministic.

## 6. Serialization contract

`scene/scene_serializer.cpp` uses a hand-written JSON tokenizer/parser:

- **Strict:** unknown fields are a load error. Adding a field requires a parser
  branch.
- **No ids, no camera** are serialized. Load assigns fresh sequential ids and
  preserves the existing camera.
- Numbers are written with **4 decimal places** (`std::fixed << setprecision(4)`)
  so files diff well.
- Component serialization is an **explicit per-type fork** (`dynamic_cast` on
  save, `type == "..."` on load). There is intentionally no registry/factory.
- `scene.load` replaces all non-camera objects and clears selection.

## 7. DebugState substring-stability rule

`DebugState::build` output is effectively an API. Treat it as such:

- Many unit tests in `src/tests/main.cpp` do `text.find("...")` on it (e.g.
  `[1] Camera`, `[2] Cube`, `selected: Cube [2]`, `renderCommands: 2`,
  `components: MeshRenderer`, `SceneFile:`, `Last Script:`, etc.).
- **One test does an exact round-trip** (`writeToFile` then `content == text`).

Therefore any change to `DebugState` must be **purely additive** (new lines/fields
are fine; never reorder, rename, or reformat existing output). When adding a
field, add a unit test that searches for it.

## 8. Adding a Component — checklist

There is intentionally no registry. Adding a component type touches, at minimum:

1. `src/engine/scene/<name>.h` / `.cpp` — the class (`typeName()`, `clone()`,
   optional `onStart`/`onUpdate`).
2. `scene/scene_serializer.cpp` — save branch (`dynamic_cast`) and load branch
   (`type == "..."`).
3. `editor/editor.mm` — inspector block + "Add" button (if user-facing).
4. `debug/debug_state.cpp` — per-component detail line (additive; see §7).
5. `agent/command.cpp` — handler(s) + `commandTable()` entry/entries, if it needs
   commands.
6. `src/tests/main.cpp` — defaults, `clone()`, lifecycle, serialization
   round-trip, command success/error cases.
7. A `assets/tests/<name>.wbs` script and, if useful, a
   `assets/tests/examples/<name>_fail.wbs`.

No build file changes are needed for new files under `src/engine/scene/` (the
`sandbox` target globs them); the `tests` target lists files explicitly, so add
new `.cpp`/`.h` pairs to `premake5.lua` if the tests target must compile them.

## 9. Adding an agent command — checklist

1. Write a handler function in `agent/command.cpp` returning
   `AgentCommandResult` (use `makeSuccess` / `makeError` /
   `makeAssertionFailure`).
2. Add an entry to the static `commandTable()` with `name`, `usage`,
   `description`, `example`. Discovery via `agent.commands` / `agent.help` is then
   automatic.
3. Assertion failures (not usage errors) must call `makeAssertionFailure(ctx, …)`
   so they set `ctx.lastAssertionFailure` and stop scripts.
4. Add unit tests in `src/tests/main.cpp` for success and each error case, and a
   search assertion in the "New commands appear in command discovery" test.
5. Document the command in `docs/agent_interface.md` and, if relevant,
   `docs/assertions.md` or `docs/test_suites.md`.
