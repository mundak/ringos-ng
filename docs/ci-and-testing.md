# CI And Testing

This document defines the current verification contract for ringos-ng. The goal
is to keep local development, containerized Windows workflows, and GitHub
Actions aligned around the same Linux toolchain and the same CMake and CTest
entry points.

## Execution Environments

- CI runs on `ubuntu-latest`.
- Windows users are expected to use the Docker-based wrappers in `scripts/`.
- Native Linux is supported for direct shell iteration and GDB-based debugging.

The repository should keep one canonical build and test interface even when it
is executed through different shells.

## Source Of Truth

The build and test contract lives in the repository, not in ad hoc CI shell
logic.

Use these layers:

1. `CMakePresets.json` defines configure, build, and test presets.
2. `CTest` defines the smoke-test surface.
3. `scripts/*.sh` contains QEMU launch and output assertion logic.
4. `scripts/docker-*.bat` wraps the Windows container workflow.
5. `.github/workflows/ci.yml` installs dependencies and runs the same presets.

The workflow file should stay thin. If a command matters for local users, it
should exist in the repository first.

## Presets In Use

Configure presets:

- `x64-debug`
- `arm64-debug`
- `x64-ci`
- `arm64-ci`

Build presets:

- `build-x64-debug`
- `build-arm64-debug`
- `build-x64-ci`
- `build-arm64-ci`

Test presets:

- `test-x64-debug`
- `test-arm64-debug`
- `test-x64-ci`
- `test-arm64-ci`

Debug presets are intended for direct local iteration. CI presets should remain
deterministic and automation-friendly.

## Local Workflows

### Windows Through Docker

Build and run a target in QEMU:

```bat
scripts\docker-run-os-x64.bat
scripts\docker-run-os-arm64.bat
```

Build and smoke-test a target:

```bat
scripts\docker-test-x64.bat
scripts\docker-test-arm64.bat
```

These wrappers rebuild the shared `ringos-ci` image from
`docker/Dockerfile`, then run the requested configure, build, and test or run
sequence inside the container.

### Native Linux

Install the same core packages used by `docker/Dockerfile`, including
`gdb-multiarch` for the debugger-launch and debug-host test surface.

Configure and build:

```bash
cmake --preset x64-debug
cmake --build --preset build-x64-debug

cmake --preset arm64-debug
cmake --build --preset build-arm64-debug
```

Run smoke tests:

```bash
ctest --preset test-x64-debug
ctest --preset test-arm64-debug
```

Run QEMU directly:

```bash
scripts/run-x64.sh build/x64-debug/arch/x64/ringos_x64
scripts/run-arm64.sh build/arm64-debug/arch/arm64/ringos_arm64
```

Debug with QEMU's GDB stub:

```bash
scripts/debug-x64.sh build/x64-debug/arch/x64/ringos_x64
gdb-multiarch -ex "target remote :1234" build/x64-debug/arch/x64/ringos_x64.elf64

scripts/debug-arm64.sh build/arm64-debug/arch/arm64/ringos_arm64
gdb-multiarch -ex "target remote :1234" build/arm64-debug/arch/arm64/ringos_arm64
```

Set `RINGOS_GDB_PORT` to move the stub off the default port. Set
`RINGOS_QEMU_BIN` when tests or local tooling need to intercept the QEMU launch
without editing the shared scripts.

On x64, set `RINGOS_DEBUGCON` to redirect the port `0xe9` debug console sink
when you want the debugger-only channel somewhere other than the default host
stderr stream.

The shared debugger-oriented logging hook is `debug_semihost_log()`. The arm64
debug wrapper enables semihosting with `target=gdb`, while the x64 debug
wrapper exposes the same hook through QEMU's x86 debug console on port `0xe9`.
The direct run wrappers also configure host-visible debug sinks so boot-time
logs no longer depend on a shared serial console abstraction.

## Smoke Test Contract

Each smoke test script should:

1. Launch the correct QEMU target in headless mode.
2. Capture host-side debug output from the selected launch wrapper.
3. Apply a hard timeout.
4. Assert the expected output appears.
5. Return a non-zero exit code on timeout, early crash, or missing output.

The debugger launch wrappers should also stay under test. Their contract is to:

1. launch the correct QEMU binary for the active architecture
2. expose a GDB stub on the configured port
3. pause execution at startup with `-S`
4. preserve the same host-side debug sink configuration used by local debugging

Keep raw QEMU command lines inside scripts so the same execution path is used by
developers, wrappers, and CTest.

## CI Contract

The current GitHub Actions workflow should continue to:

1. install the Linux dependency set
2. configure x64
3. build x64
4. run the x64 smoke test preset
5. configure arm64
6. build arm64
7. run the arm64 smoke test preset

If the dependency stack changes, update `docker/Dockerfile` and
`.github/workflows/ci.yml` together so local container runs and CI stay in
sync.
