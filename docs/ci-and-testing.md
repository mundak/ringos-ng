# CI And Testing Layout

This document defines the recommended verification layout for milestone one so
the same build and test flow works both locally in WSL2 and in GitHub Actions.

## Goals

1. Keep one canonical execution environment: Linux.
2. Use WSL2 locally because the repository already targets that workflow.
3. Use the same commands locally and in CI.
4. Keep early CI minimal:
   - configure both targets
   - build both targets
   - run both smoke tests

## Canonical Environment

- Local development environment: Ubuntu in WSL2
- CI environment: `ubuntu-latest` on GitHub Actions

Do not maintain separate Windows-native and Linux CI flows during milestone one.
If commands only work in one of those environments, they will drift.

## Source Of Truth

The build and test contract should live in the repository itself, not in the
workflow YAML.

Use these layers:

1. `CMakePresets.json`
   - defines configure and build presets
2. `CTest`
   - defines smoke tests
3. `scripts/`
   - contains the QEMU launch and output assertion scripts
4. `.github/workflows/ci.yml`
   - installs dependencies and runs the same presets and tests as local users

The workflow file should be thin. It should not contain bespoke build logic.

## Recommended Repository Layout

When the build skeleton is created, use this shape:

```text
.
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/
│   ├── toolchains/
│   │   ├── x64.cmake
│   │   └── arm64.cmake
│   └── tests/
│       └── smoke_tests.cmake
├── scripts/
│   ├── run-x64.sh
│   ├── run-arm64.sh
│   ├── test-smoke-x64.sh
│   └── test-smoke-arm64.sh
└── .github/
    └── workflows/
        └── ci.yml
```

Keep all emulator invocations in scripts so raw QEMU commands do not become
tribal knowledge.

## Preset Strategy

Use presets to standardize the commands developers and CI run.

Recommended configure presets:

- `x64-debug`
- `arm64-debug`
- `x64-ci`
- `arm64-ci`

Recommended build presets:

- `build-x64-debug`
- `build-arm64-debug`
- `build-x64-ci`
- `build-arm64-ci`

Recommended test presets:

- `test-x64-debug`
- `test-arm64-debug`
- `test-x64-ci`
- `test-arm64-ci`

The debug presets are for local iteration. The CI presets should disable any
non-essential developer options and use deterministic paths suitable for
automation.

## Example CMakePresets.json

This is the target structure to aim for once the top-level CMake project exists.

```json
{
  "version": 6,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 27,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "RINGOS_ENABLE_TESTING": "ON"
      }
    },
    {
      "name": "x64-debug",
      "inherits": "base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "RINGOS_TARGET_ARCH": "x64"
      }
    },
    {
      "name": "arm64-debug",
      "inherits": "base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "RINGOS_TARGET_ARCH": "arm64"
      }
    },
    {
      "name": "x64-ci",
      "inherits": "x64-debug",
      "cacheVariables": {
        "RINGOS_CI": "ON"
      }
    },
    {
      "name": "arm64-ci",
      "inherits": "arm64-debug",
      "cacheVariables": {
        "RINGOS_CI": "ON"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "build-x64-debug",
      "configurePreset": "x64-debug"
    },
    {
      "name": "build-arm64-debug",
      "configurePreset": "arm64-debug"
    },
    {
      "name": "build-x64-ci",
      "configurePreset": "x64-ci"
    },
    {
      "name": "build-arm64-ci",
      "configurePreset": "arm64-ci"
    }
  ],
  "testPresets": [
    {
      "name": "test-x64-debug",
      "configurePreset": "x64-debug",
      "output": {
        "outputOnFailure": true
      }
    },
    {
      "name": "test-arm64-debug",
      "configurePreset": "arm64-debug",
      "output": {
        "outputOnFailure": true
      }
    },
    {
      "name": "test-x64-ci",
      "configurePreset": "x64-ci",
      "output": {
        "outputOnFailure": true
      },
      "execution": {
        "stopOnFailure": true
      }
    },
    {
      "name": "test-arm64-ci",
      "configurePreset": "arm64-ci",
      "output": {
        "outputOnFailure": true
      },
      "execution": {
        "stopOnFailure": true
      }
    }
  ]
}
```

If you want a single command for pre-push verification later, add a thin wrapper
script that invokes both per-architecture CTest presets.

## CTest Layout

Register the smoke tests in CMake so CTest is the standard test runner.

Example:

```cmake
include(CTest)

if(BUILD_TESTING)
  add_test(
    NAME smoke_x64
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-smoke-x64.sh
            $<TARGET_FILE:ringos_x64>
  )

  set_tests_properties(
    smoke_x64
    PROPERTIES
      TIMEOUT 20
  )

  add_test(
    NAME smoke_arm64
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-smoke-arm64.sh
            $<TARGET_FILE:ringos_arm64>
  )

  set_tests_properties(
    smoke_arm64
    PROPERTIES
      TIMEOUT 20
  )
endif()
```

Keep the QEMU command lines in scripts, not in `add_test()`, so developers can
also run the exact same launch path outside CTest.

## Smoke Test Contract

Each smoke test script should do only these things:

1. Launch the correct QEMU target in headless mode.
2. Redirect serial output to stdout or a temporary log file.
3. Apply a hard timeout.
4. Assert the expected output appears.
5. Return a non-zero exit code on timeout, early crash, or missing output.

Expected milestone-one assertions:

- `smoke_x64`
  - x64 boot banner appears
  - shared kernel entry is reached
  - hello world appears on serial
- `smoke_arm64`
  - arm64 boot banner appears
  - shared kernel entry is reached
  - hello world appears on serial

Do not make the smoke tests interactive. They must run unattended in CI.

## Script Shape

The run scripts should be thin wrappers around the QEMU invocation. The test
scripts should call the run scripts and check output.

Suggested responsibilities:

- `scripts/run-x64.sh`
  - launch x64 QEMU with the built image and serial redirected
- `scripts/run-arm64.sh`
  - launch arm64 QEMU virt with the built image and serial redirected
- `scripts/test-smoke-x64.sh`
  - call `run-x64.sh`, capture output, enforce timeout, assert expected text
- `scripts/test-smoke-arm64.sh`
  - call `run-arm64.sh`, capture output, enforce timeout, assert expected text

That split keeps the run path reusable for manual local debugging.

## Local Commands

Once the presets exist, the local workflow should be:

```bash
cmake --preset x64-debug
cmake --build --preset build-x64-debug
ctest --preset test-x64-debug
```

And for arm64:

```bash
cmake --preset arm64-debug
cmake --build --preset build-arm64-debug
ctest --preset test-arm64-debug
```

For pre-push verification, run both configure and build presets, then run the
full smoke-test set.

## GitHub Actions Structure

Use one Linux-only workflow during milestone one.

Recommended job responsibilities:

1. Check out the repository.
2. Install CMake, Ninja, compilers, and QEMU packages.
3. Configure x64 and arm64 with the CI presets.
4. Build x64 and arm64.
5. Run the smoke tests with CTest.

Example layout:

```yaml
name: ci

on:
  pull_request:
  push:
    branches:
      - main

jobs:
  build-and-test:
    runs-on: ubuntu-latest

    steps:
      - name: Check out repository
        uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            cmake \
            ninja-build \
            qemu-system-arm \
            qemu-system-misc \
            qemu-system-x86 \
            gcc \
            g++

      - name: Configure x64
        run: cmake --preset x64-ci

      - name: Build x64
        run: cmake --build --preset build-x64-ci

      - name: Configure arm64
        run: cmake --preset arm64-ci

      - name: Build arm64
        run: cmake --build --preset build-arm64-ci

      - name: Run smoke tests
        run: |
          ctest --preset test-x64-ci
          ctest --preset test-arm64-ci
```

Keep the workflow as a consumer of the repository-defined interface. If CI has
special command lines that local development does not use, that is a design
failure.

## Review Standard For Changes

Use this rule when testing changes:

1. If a change is architecture-local, run that target's build and smoke test.
2. If a change touches shared startup code, common kernel code, or build logic,
   run both targets locally.
3. CI always runs both targets before merge.

This keeps local iteration fast while still protecting the milestone contract.

## Follow-Up Implementation Order

When the repository gains its initial CMake build skeleton, implement this in
the following order:

1. Add `CMakeLists.txt` and `CMakePresets.json`.
2. Add stable x64 and arm64 run scripts.
3. Add x64 and arm64 smoke-test scripts.
4. Register the smoke tests with CTest.
5. Add `.github/workflows/ci.yml` that runs the same preset and CTest flow.

That order keeps the workflow file last, where it belongs.
