# Plan: Split `src/tests/main.cpp`

## Goal

Split the large `src/tests/main.cpp` (~2275 lines) into focused per-subsystem test files so future agent work does not require reading one giant file.

## Constraints

- No engine behavior changes.
- No test behavior changes.
- No new test framework.
- Keep simple `assert`-based tests.
- Preserve existing test coverage.
- Keep `./test.sh` output simple.
- Keep all tests green.

## Proposed file layout

```
src/tests/
  main.cpp                 // entry point only: forward declarations + function calls
  test_core.cpp            // ObjectId, Transform, Scene basics, CLI, Settings
  test_scene.cpp           // GameObject/Scene operations, component lifecycle, hierarchy
  test_agent.cpp           // agent commands, script runner, assertions
  test_renderer.cpp        // mesh geometry, normals, normal matrix, RenderContext, MeshRenderer
  test_serialization.cpp   // all save/load, round-trip, and backward-compatibility tests
  test_picking.cpp         // ray/object picking
  test_debug.cpp           // DebugState output
  test_test_suite.cpp      // TestSuite discovery / pass-fail / failure bundle
```

No `test_helpers.h/.cpp` is needed; each file keeps its own includes.

## Split mapping

### `test_core.cpp`

| Lines (old) | Test topic |
|-------------|------------|
| 34-46 | ObjectId uniqueness via Scene allocation |
| 49-64 | Transform defaults |
| 66-79 | Scene object creation |
| 759-793 | CLI option parsing |
| 796-900 | Settings round-trips (window size, position, editor state) |

### `test_scene.cpp`

| Lines (old) | Test topic |
|-------------|------------|
| 197-223 | Scene authoring operations (duplicate, delete) |
| 903-931 | Component lifecycle: onStart once, onUpdate each step |
| 933-953 | Component lifecycle: inactive objects are skipped |
| 955-961 | RotateComponent defaults and typeName |
| 963-981 | RotateComponent::onUpdate advances rotation |
| 982-993 | RotateComponent::clone preserves angular velocity |
| 1029-1045 | sim.step advances the simulation deterministically |
| 1627-2071 | Milestone 0.6: Transform Hierarchy (all hierarchy tests) |

### `test_agent.cpp`

| Lines (old) | Test topic |
|-------------|------------|
| 116-194 | Agent command interface (scene.list, scene.select, scene.get_selected, transform.set_position, transform.get, debug.dump, unknown) |
| 196-270 | Command discovery |
| 275-312 | Scene authoring agent commands |
| 313-337 | Component agent commands |
| 380-408 | Script runner: success, blank lines, comments, scene mutation |
| 410-430 | Script runner: stops on first failure |
| 432-451 | script.run agent command |
| 591-680 | Assertion commands (object_count, object_exists, selected, has_component) |
| 682-697 | Script runner: assertion failure stops execution |
| 994-1027 | component.add_rotator and component.set_rotator commands |
| 1047-1065 | assert.rotation success and failure |
| 1084-1102 | assert.position success and failure |
| 1117-1134 | assert.scale success and failure |
| 1136-1173 | assert.color success, failure, and missing MeshRenderer |
| 1263-1274 | New rotator commands appear in command discovery |
| 1598-1622 | component.set_mesh command |
| 1624-1658 | assert.mesh command |
| 1698-1721 | New mesh commands appear in command discovery and have help |

### `test_renderer.cpp`

| Lines (old) | Test topic |
|-------------|------------|
| 81-90 | RenderContext command accumulation |
| 1277-1378 | Mesh geometry generation (cube, sphere, plane) |
| 1380-1383 | Vertex layout for lighting |
| 1385-1428 | Normal matrix: rotation + uniform scale |
| 1430-1468 | Normal matrix: non-uniform scale |
| 1470-1489 | Mesh shape string mapping |
| 1491-1501 | MeshRenderer defaults and clone preserve shape |
| 1670-1696 | drawShape maps MeshShape to ShapeType |

### `test_serialization.cpp`

| Lines (old) | Test topic |
|-------------|------------|
| 224-272 | Scene save/load |
| 358-368 | Load warns about objects with no MeshRenderer |
| 1176-1204 | RotateComponent serialization round-trip |
| 1503-1561 | Serialization round-trip preserves mesh shape |
| 1563-1587 | Serialization backward compatibility: missing "mesh" defaults to cube |
| 1589-1596 | Serialization: unknown mesh value fails to load |
| 1808-1850 | Serialization round-trip with nested children |
| 1852-1870 | Serialization backward compatibility: a flat scene writes no "children" field |

### `test_picking.cpp`

| Lines (old) | Test topic |
|-------------|------------|
| 2073-2271 | Milestone 0.9: Object Picking (all picking tests) |

### `test_debug.cpp`

| Lines (old) | Test topic |
|-------------|------------|
| 92-115 | DebugState formatting |
| 369-378 | DebugState lists components |
| 380-388 | DebugState includes loaded scene filename |
| 453-460 | DebugState reports last script path |
| 507-514 | DebugState reports last capture path |
| 583-590 | DebugState reports last bundle path |
| 674-681 | DebugState reports last assertion failure |
| 1206-1218 | DebugState lists RotateComponent with angular velocity |
| 1660-1668 | DebugState reports mesh shape for MeshRenderer |
| 1872-1889 | DebugState: Hierarchy section, parent: and worldPosition: lines |

### `test_test_suite.cpp`

| Lines (old) | Test topic |
|-------------|------------|
| 683-697 | Test suite discovery |
| 699-755 | Test suite pass/fail counting |
| 737-757 | Test suite failure bundle writes state.txt |

### `main.cpp`

Forward-declare the runner functions and call them in order:

```cpp
void runTestCore();
void runTestScene();
void runTestAgent();
void runTestRenderer();
void runTestSerialization();
void runTestPicking();
void runTestDebug();
void runTestTestSuite();

int main()
{
    runTestCore();
    runTestScene();
    runTestAgent();
    runTestRenderer();
    runTestSerialization();
    runTestPicking();
    runTestDebug();
    runTestTestSuite();

    std::printf("All tests passed.\n");
    return 0;
}
```

## Build changes

Update `premake5.lua` project `"tests"` `files` block to list the new `.cpp` files alongside `src/tests/main.cpp`.

## Mechanical move rules

1. Copy each block verbatim; do not rewrite assertions.
2. Keep block comments intact so history remains readable.
3. Add only the includes each test file actually needs.
4. Preserve `using scene::MeshRenderer;` in files that use it.
5. Keep `std::printf("All tests passed.\n");` only in `main.cpp`.

## Verification

After the split, run:

```bash
./test.sh
./build.sh
./build/debug/sandbox --run-tests --exit
```

Expected outcome: all commands succeed with no new warnings and the same `"All tests passed."` output.
