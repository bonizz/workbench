# Test Suites

Test suites let agents run a collection of `.wbs` scripts and get a single PASS/FAIL summary.

## Directory layout

Place test scripts in:

```text
assets/tests/
```

Each file must end with `.wbs`. Example:

```text
assets/tests/
├── create_cube.wbs
├── duplicate_object.wbs
├── components.wbs
└── missing_object.wbs
```

## Running tests

From the CLI:

```bash
./build/debug/sandbox --run-tests
./build/debug/sandbox --run-tests --exit
```

## Output

```text
PASS components
PASS create_cube
PASS duplicate_object
FAIL missing_object
      Object not found: MissingCube

Summary:
Passed: 3
Failed: 1
```

Exit codes:

- `0` — all tests passed
- `1` — one or more tests failed

## Execution model

1. Every `.wbs` file in `assets/tests/` is discovered and sorted by filename.
2. For each test, the engine creates a fresh scene containing only the default camera.
3. The script runs through the existing Script Runner.
4. A failing assertion or invalid command marks the test as failed but does **not** stop the suite.
5. After all tests finish, the summary is printed.

## Failure artifacts

For every failing test, the engine creates:

```text
bundles/test_failures/<test_name>/
```

Currently this directory contains only `state.txt`, written synchronously. Screenshots are omitted in this milestone to avoid coordinating multiple async captures; the failure message and state dump are the primary debugging aid.

Example:

```text
bundles/test_failures/missing_object/
└── state.txt
```

## Writing a test

A test script is a normal `.wbs` script. Use assertions to verify behavior:

```text
scene.create_cube CubeA
assert.object_exists CubeA
assert.has_component CubeA MeshRenderer
```

Because each test starts with a fresh scene, the default camera is the only object present at the start. The first created object receives ObjectId 2.

## Agent workflow

1. Create or edit `assets/tests/<name>.wbs`.
2. Run `./build/debug/sandbox --run-tests --exit`.
3. If the suite fails, inspect `bundles/test_failures/<name>/state.txt` for the failing state.

## Limitations

- Tests are discovered only from `assets/tests/`.
- Only `.wbs` files are executed.
- Failure bundles contain `state.txt` only; no screenshot is captured.
- Tests do not run in parallel.
- Test scripts must clean up any files they write if they want to avoid cross-test interference.
