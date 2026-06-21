Workbench Agent Guidelines

Project Goals

Workbench is a learning-focused game engine built in C++ and Metal.

The primary goal is understanding engine architecture through implementation.

The code should favor readability, simplicity, and explicitness over abstraction and optimization.

Start Here / Docs Map

Read these before touching the engine. They encode the conventions and
contracts that source files alone do not surface.

* `docs/conventions.md` — READ FIRST. Determinism model, camera special rules,
  math/units (radians vs degrees), id-vs-name addressing, serialization contract,
  DebugState stability rule, and the component/command extension checklists.
* `docs/scenes.md` — GameObject/Transform data model and scene file format.
* `docs/components.md` — Component model, lifecycle (`onStart`/`onUpdate`),
  `MeshRenderer`, `RotateComponent`.
* `docs/agent_interface.md` — the full agent command surface and context struct.
* `docs/test_suites.md`, `docs/assertions.md`, `docs/scripts.md`,
  `docs/cli_automation.md` — how tests and automation run.
* `docs/observability.md`, `docs/capture.md`, `docs/bundles.md` — DebugState and
  repro artifacts (the primary agent feedback loop).
* `docs/plans/` — milestone plans. The architectural decision log (read the
  latest before proposing changes).

Source layout: `src/engine/{scene,agent,renderer,editor,debug,core,platform,capture}`.
Key entry points: `scene/scene.h`, `scene/game_object.h`, `scene/transform.h`,
`scene/component.h`, `agent/command.cpp` (the `commandTable()` catalogs engine
capabilities), `core/application.cpp` (the frame + automation loop).

Design Principles

* Prefer simple solutions first.
* Avoid introducing systems before they are needed.
* Refactor when patterns emerge.
* Keep dependencies minimal.
* Optimize only after measuring.

Current Architecture Preferences

* Traditional GameObject + Component model.
* No ECS unless a real performance problem justifies it.
* Clear ownership and lifetime management.
* Stable object IDs for serialization.
* ImGui for tooling and debugging.

When Implementing Features

Before adding a new system:

1. Explain why the system is needed.
2. Describe simpler alternatives considered.
3. Keep implementations minimal.
4. Avoid speculative extensibility.

Code Style

* Modern C++
* Prefer composition over inheritance
* Minimize macros
* Favor explicit code over clever code
* Small focused classes

Communication

When proposing architectural changes:

* Explain tradeoffs.
* Explain what future problem the design solves.
* Call out complexity costs.
* Prefer iterative improvements.

Screenshot Validation Policy

Screenshot capture and image analysis are currently disabled unless explicitly
requested by the user.

For validation, prefer this order:

1. Unit tests
2. `.wbs` integration tests
3. Assertions
4. DebugState inspection
5. Manual user verification (only when the user explicitly asks to check visually)

Do not automatically capture screenshots or analyze rendered images.

Only use screenshot capture or inspection when explicitly requested by the user.
Even final smoke tests should not use screenshots unless the user asks for one.

Token Efficiency Guidelines

When implementing a milestone:

* Do not read large files fully unless necessary.
* Prefer targeted search first (e.g., grep/ripgrep for a symbol or command name).
* For command changes, inspect the command table and nearby related handler only.
* For tests, inspect nearby tests for the same subsystem only.
* For prior milestone plans, read the goal/constraints/contracts first, not the whole file.
* Ask for clarification if a file or subsystem is unclear instead of scanning broadly.
* Avoid screenshot analysis unless explicitly requested.
* Build and test with `./build.sh` and `./test.sh`. When their output is piped
  (the usual agent case) they are quiet: a single `build OK` / `tests OK` line on
  success, or the failing log tail on error. Do not wrap them in your own
  output-capturing redirect, and do not re-run in a verbose mode to read errors —
  the failure tail already contains them. Pass `VERBOSE=1` only if you genuinely
  need full progress output. Note: the `tests` target does not compile
  `application.cpp` / `window.mm`; run `./build.sh` to verify those.
