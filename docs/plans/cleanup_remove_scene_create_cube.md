# Plan: Remove `scene.create_cube` (superseded by `scene.create_primitive`)

Status: IMPLEMENTED in working tree (uncommitted). `scene.create_cube` /
`cmdSceneCreateCube` are removed from `command.cpp`, and the `.wbs` scripts,
tests, and docs are migrated to `scene.create_primitive cube`. Pending commit.

## Goal

Remove the `scene.create_cube` agent command entirely. It is now fully
redundant with `scene.create_primitive cube`, which was added in the
"Create ▾ editor popup" change (see `editor.mm` `createPrimitive` and
`command.cpp` `cmdSceneCreatePrimitive`). Keeping a single creator command:

- Shrinks the creation surface back to one orthogonal primitive creator.
- Removes a maintenance hazard: `cmdSceneCreateCube` and `cmdSceneCreatePrimitive`
  share a `createPrimitiveObject(...)` helper today, but a second entry point
  is one more place the default name/transform/color can drift.
- Keeps `agent.commands` / docs / tests honest about what the engine actually
  exposes.

`scene.create_primitive <shape> [name]` is a strict superset:

| `scene.create_cube` behavior        | `scene.create_primitive cube` equivalent |
|-------------------------------------|------------------------------------------|
| name defaults to `"Cube"`           | name defaults to `"Cube"` (capitalized shape) |
| transform `{0,0.5,0}`, scale `0.5`  | identical (shared `createPrimitiveObject`) |
| default color `{0.95,0.55,0.20,1}`  | identical (shared `createPrimitiveObject`) |
| sets `ctx.selected`                 | identical |

So every `scene.create_cube <name>` maps 1:1 to
`scene.create_primitive cube <name>` with no observable difference.

## Non-goals

- **No engine behavior changes.** The only output difference should be the
  command name in `agent.commands` output and help text.
- **No saved-scene migration.** `scene.create_cube` is an *authoring* command
  and is never written into `.scene` JSON (verified: `grep -rln create_cube
  assets/scenes/` is empty). Removing it cannot break existing saved scenes.
- **No change to `MeshRenderer`, serialization, or the renderer.**
- **No deprecation/alias period.** This is an internal engine; remove in one
  step. (Alternative considered and rejected — see §Risks.)
- **No re-touching the editor UI.** The `Create ▾` popup already uses the
  shared helper, not the command.

## Constraints

- All tests stay green: `./test.sh` and `./build.sh`.
- The `.wbs` rewrite is mechanical (string substitution), not a re-authoring.
- Preserve test intent: assertion values, object counts, and names must not
  change.

---

## 1. Scope (full inventory)

Every reference, verified against current source. Grouped by file category.

### A. Engine source (1 file)

| File | Line(s) | What |
|------|---------|------|
| `src/engine/agent/command.cpp` | ~402 | `cmdSceneCreateCube` handler function — delete |
| `src/engine/agent/command.cpp` | ~1140-1144 | `commandTable()` entry `scene.create_cube` — delete |

The file-local helper `createPrimitiveObject(...)` in the anonymous namespace
must **stay** — `cmdSceneCreatePrimitive` still uses it.

### B. Unit tests (2 files)

| File | Line(s) | Current | Replace with |
|------|---------|---------|--------------|
| `src/tests/test_agent.cpp` | 110 | `scene.create_cube TestA` | `scene.create_primitive cube TestA` |
| `src/tests/test_agent.cpp` | 207 | `scene.create_cube ScriptCube` | `scene.create_primitive cube ScriptCube` |
| `src/tests/test_agent.cpp` | 235 | `scene.create_cube A` | `scene.create_primitive cube A` |
| `src/tests/test_agent.cpp` | 257 | `scene.create_cube AgentCube` | `scene.create_primitive cube AgentCube` |
| `src/tests/test_agent.cpp` | 341 | `scene.create_cube Cube` | `scene.create_primitive cube Cube` |
| `src/tests/test_test_suite.cpp` | 34 | `scene.create_cube TestCube` | `scene.create_primitive cube TestCube` |

Note: the dedicated `scene.create_cube` test block (around line 110) is the
*scene-authoring* test. After the rewrite it still exercises
`create_primitive`, so coverage of the cube path is preserved. **Do not**
delete the block — just change the command string.

### C. `.wbs` test/script fixtures (mechanical rewrite)

`scene.create_cube <name>` → `scene.create_primitive cube <name>` in every file:

```
assets/scripts/assertion_pass.wbs:1
assets/scripts/create_test_scene.wbs:3,6
assets/tests/color.wbs:1
assets/tests/components.wbs:1
assets/tests/duplicate_object.wbs:1
assets/tests/examples/color_fail.wbs:1
assets/tests/examples/hierarchy_cycle_fail.wbs:1,2
assets/tests/examples/mesh_unknown_fail.wbs:1
assets/tests/examples/position_fail.wbs:1
assets/tests/examples/rotation_fail.wbs:1
assets/tests/examples/scale_fail.wbs:1
assets/tests/hierarchy_detach.wbs:1,3
assets/tests/hierarchy_parent.wbs:1,3
assets/tests/hierarchy_rotate.wbs:1,3
assets/tests/mesh_persistence.wbs:1
assets/tests/mesh_shapes.wbs:1,4,8
assets/tests/position.wbs:1
assets/tests/rotator_multi_axis.wbs:1
assets/tests/rotator.wbs:1
assets/tests/scale.wbs:1
```

### D. The `create_cube.wbs` fixture — rename or keep?

**Decision: rename `assets/tests/create_cube.wbs` → `assets/tests/create_primitive.wbs`.**

Rationale: the test name shows up in `./build/debug/sandbox --run-tests` PASS
lines and in `docs/test_suites.md`. A file called `create_cube.wbs` testing
`scene.create_primitive` is misleading. Test-suite discovery is
directory-based (`test_suite.cpp:39` scans `assets/tests/` for `*.wbs`, sorted
by filename), so renaming is safe — it just re-sorts alphabetically
(`create_primitive.wbs` still lands before `duplicate_object.wbs`).

The renamed file's contents also get the mechanical rewrite
(`scene.create_cube CubeA` → `scene.create_primitive cube CubeA`).

### E. Docs (5 files)

| File | Line(s) | Change |
|------|---------|--------|
| `docs/agent_interface.md` | 52 | Delete the `scene.create_cube` row. The `scene.create_primitive` row (added in the prior change) already documents the replacement. |
| `docs/scenes.md` | 41, 42, 116, 125, 128 | Rewrite the `scene.create_cube ...` example lines to `scene.create_primitive cube ...`. **Also fix line 116** — it still describes a "Create Cube" editor button, which is already stale (the UI is now a `Create ▾` popup). Replace with: *"**Create ▾** — opens a menu to add a Cube, Sphere, Plane, or Empty Object at the origin and select it."* |
| `docs/scripts.md` | 21, 24, 51 | Rewrite example lines to `scene.create_primitive cube ...`. |
| `docs/test_suites.md` | 18, 40, 83, 95, 138, 140, 143 | Rename `create_cube` → `create_primitive` in the file-tree listing (18), the PASS line (40), and rewrite the example command lines (83, 95, 138, 140, 143). |
| `docs/assertions.md` | 83, 93, 96, 104 | Rewrite example lines to `scene.create_primitive cube ...`. Also: line 93 mentions a script filename `create_cube_test.wbs` and line 104 runs it — rename the referenced script to `create_primitive_test.wbs` for consistency (and rename the actual script file under `assets/scripts/` to match, **or** keep the file and just note the rename is optional). |

The `docs/assertions.md` script rename is the one genuinely editorial choice.
**Recommendation:** rename `assets/scripts/create_cube_test.wbs` →
`create_primitive_test.wbs` to keep the doc literal and consistent. It's a
sample script, not a test-suite fixture, so no harness coupling.

---

## 2. Mechanical rewrite rules

1. **Pure substitution.** Replace the literal token `scene.create_cube` with
   `scene.create_primitive cube` (note the shape argument). Do not re-author
   fixture content, object names, or assertion values.
2. **Argument order is unchanged.** `scene.create_cube <name>` becomes
   `scene.create_primitive cube <name>` — the optional name stays last.
3. **Do not touch** the anonymous-namespace `createPrimitiveObject` helper, the
   `Create ▾` editor popup, or `cmdSceneCreatePrimitive`. Those are the
   *replacements*, not the *removals*.
4. **Delete in full** the `cmdSceneCreateCube` function body and its
   `commandTable()` entry. Do not leave a stub.
5. When editing docs, read each surrounding paragraph so prose still reads
   grammatically after the command-name swap (e.g. a sentence "use
   `scene.create_cube`" may need "use `scene.create_primitive cube`").

## 3. A safe mechanical command for the `.wbs` rewrite

The bulk of §1-C is a single sed across `assets/`:

```bash
grep -rl 'scene.create_cube' assets/scripts assets/tests \
  | xargs sed -i '' 's/scene\.create_cube /scene.create_primitive cube /g'
```

Then verify nothing was missed:

```bash
grep -rn 'scene.create_cube' assets/
```

That should print nothing. (macOS `sed -i ''` syntax; on Linux drop the `''`.)

**Do not** blindly sed `docs/` — the docs rewrite includes the stale
"Create Cube" button prose (§1-E) and a file-tree rename that need human eyes.

---

## 4. Risks and alternatives considered

### Risks / edge cases

- **A test asserts command *count*.** Verified: none does
  (`test_agent.cpp` discovery tests assert *presence*, not count). Removing one
  command cannot break them.
- **The renamed `create_primitive.wbs` collides alphabetically.** No collision;
  it sits at the top of the sort, before `duplicate_object.wbs`.
- **A stale `scene.create_cube` survives somewhere.** Mitigated by the
  §3 verification `grep` plus a full-tree grep in §5.
- **Editorial drift in docs.** The `docs/scenes.md:116` "Create Cube" button
  text is *already* wrong from the prior UI change. Fixing it here is a free
  bonus, not scope creep — but call it out in the commit message.

### Alternatives considered and rejected

- **Keep `scene.create_cube` as a deprecated alias** that internally forwards
  to `create_primitive cube`. Rejected: adds a permanent second command surface
  for no caller benefit, and this engine has no external users to deprecate
  gracefully toward. A clean cut is simpler and matches the project's "avoid
  speculative extensibility" principle (AGENTS.md).
- **Keep `create_cube.wbs` filename, just rewrite its contents.** Rejected:
  a file named `create_cube.wbs` that calls `scene.create_primitive` is
  confusing in `--run-tests` output. The rename is cheap and directory-based
  discovery makes it safe.
- **Remove the `Create ▾` popup and go back to per-shape buttons.** Out of
  scope and contrary to the just-completed UI decision.

---

## 5. Verification

After all edits, run in order:

```bash
# 1. No surviving references anywhere (docs/plans/ excluded — this plan is allowed to mention it).
grep -rn 'scene.create_cube' . | grep -v 'docs/plans/'

# 2. Build the engine (covers application.cpp / window.mm, which ./test.sh does not compile).
./build.sh

# 3. Unit + .wbs test suite.
./test.sh

# 4. Manual smoke: the replacement command still behaves identically.
./build/debug/sandbox --run-script assets/tests/create_primitive.wbs --exit
```

Expected: the grep is empty; `build.sh` prints `build OK`; `test.sh` prints
`tests OK`; the smoke run shows `Created CubeA [...]` then `PASS
create_primitive` (or equivalent).

### Acceptance criteria

- [ ] `cmdSceneCreateCube` and its `commandTable()` entry are deleted; the
      file-local `createPrimitiveObject` helper remains and is still used by
      `cmdSceneCreatePrimitive`.
- [ ] `grep -rn 'scene.create_cube' . | grep -v docs/plans/` returns nothing.
- [ ] `agent.commands` output no longer lists `scene.create_cube`.
- [ ] All `.wbs` fixtures under `assets/tests/` and `assets/scripts/` use
      `scene.create_primitive cube`.
- [ ] `create_cube.wbs` is renamed to `create_primitive.wbs`.
- [ ] `docs/agent_interface.md`, `docs/scenes.md`, `docs/scripts.md`,
      `docs/test_suites.md`, `docs/assertions.md` reference only
      `scene.create_primitive`, and `docs/scenes.md:116` describes the
      `Create ▾` popup (not a "Create Cube" button).
- [ ] `./build.sh` → `build OK`.
- [ ] `./test.sh` → `tests OK`.

## 6. Estimated touch list

- 1 engine source file (`command.cpp`)
- 2 unit-test files (`test_agent.cpp`, `test_test_suite.cpp`)
- ~21 `.wbs` fixtures (1 renamed)
- 5 doc files
- ~1 sample script rename (`assertions.md` reference)

A focused, low-risk, mostly-mechanical cleanup.
