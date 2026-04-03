# CI And Testing

This document defines the current verification contract for ringos-ng. The goal
is to keep local development, containerized Windows workflows, and GitHub
Actions aligned around the same Linux toolchain and the same CMake and CTest
entry points.

## Execution Environments

- CI runs on `ubuntu-latest`.
- Windows users are expected to use the Docker-based test wrappers in `tests/`.
- Native Linux is supported for direct shell iteration and GDB-based debugging.
- Shared installed-toolchain consumers should prefer the published
	`ringos-toolchain-<bundle-id>.zip` release asset when the expected
	bundle already exists.

The repository should keep one canonical build and test interface even when it
is executed through different shells.

## Source Of Truth

The build and test contract lives in the repository, not in ad hoc CI shell
logic.

Use these layers:

1. `CMakePresets.json` defines configure, build, and test presets.
2. `CTest` defines the smoke-test surface.
3. `CTest` also runs host-side emulator unit tests for the x64 interpreter backend.
4. `scripts/*.sh` contains shared QEMU run/debug helpers and toolchain helpers.
5. `tests/*.sh` contains test assertions and deterministic wrapper checks.
6. `tests/docker-*.bat` wraps the Windows container workflow.
7. `.github/workflows/*.yml` installs dependencies and runs the same presets.
8. `tools/toolchain/ensure-toolchain-release.sh` resolves and downloads the
	shared installed-toolchain bundle for test and run flows, while the dedicated
	toolchain workflow handles build and publish operations.

The workflow file should stay thin. If a command matters for local users, it
should exist in the repository first.

## Presets In Use

Configure presets:

- `x64-debug`
- `arm64-debug`

Build presets:

- `build-x64-debug`
- `build-arm64-debug`

Test presets:

- `x64_emulator_unit`
- `x64_win32_loader_unit`
- `sample_hello_world_x64_native`
- `sample_hello_world_cpp_x64_native`
- `sample_console_service_write_x64_native`
- `sample_hello_world_arm64_native`
- `sample_hello_world_cpp_arm64_native`
- `sample_console_service_write_arm64_native`
- `sample_hello_world_arm64_x64_emulator`
- `sample_hello_world_cpp_arm64_x64_emulator`
- `sample_console_service_write_arm64_x64_emulator`

Each test preset maps to exactly one distinct CI scenario and stops on the
first failure.

## Local Workflows

### Windows Through Docker

Build and run a target in QEMU:

```bat
scripts\docker-run-os-x64.bat
scripts\docker-run-os-arm64.bat
```

Build and smoke-test a target:

```bat
tests\docker-test-hello-world-x64-native.bat
tests\docker-test-hello-world-cpp-x64-native.bat
tests\docker-test-console-service-write-x64-native.bat
tests\docker-test-hello-world-arm64-native.bat
tests\docker-test-hello-world-cpp-arm64-native.bat
tests\docker-test-console-service-write-arm64-native.bat
tests\docker-test-hello-world-arm64-x64-emulator.bat
tests\docker-test-hello-world-cpp-arm64-x64-emulator.bat
tests\docker-test-console-service-write-arm64-x64-emulator.bat
```

These wrappers rebuild the shared `ringos-ci` image from
`docker/Dockerfile`, then run the requested configure, build, and test or run
sequence inside the container.

Stage 8 toolchain bring-up currently starts from the Linux shell inside that
same container image:

```bash
scripts/build-clang-toolchain.sh
tools/toolchain/build-toolchain-local.sh
scripts/build-bootstrap-hosted-c.sh x64
scripts/build-bootstrap-hosted-c.sh arm64
scripts/build-bootstrap-hosted-cpp.sh x64
scripts/build-bootstrap-hosted-cpp.sh arm64
```

The first script builds the repo-owned host Clang toolchain scaffolding. The
second script reuses a persistent previous-stage LLVM cache when one is
available, then rebuilds the installed RingOS toolchain bundle against that
cached compiler. The hosted C and bootstrap hosted C++ sample scripts resolve
the published installed-toolchain bundle and compile against the downloaded
compiler configs and sysroot for the selected target. Those sample lanes no
longer depend on repo-local staged bootstrap config files or arbitrary host
compiler paths.

For Windows Docker iteration on native LLVM patches, use:

```bat
scripts\docker-build-toolchain-local.bat
```

That wrapper bind-mounts the worktree into `/workspace`, mounts a persistent
host cache into `/root/.cache/ringos`, normalizes the touched shell scripts to
LF inside the container, and then runs `tools/toolchain/build-toolchain-local.sh`.
The cache preserves the LLVM clone, Ninja build tree, and previous-stage
compiler install under `/root/.cache/ringos/native-llvm-toolchain-local`, so
subsequent patch iterations do not restart the LLVM bootstrap from scratch.

Before configuring, each wrapper now mounts a host-side cache directory into the
container and runs `tools/toolchain/ensure-toolchain-release.sh` so the
installed-toolchain bundle is fetched from GitHub Releases and the wrapper fails
immediately if the expected release is missing or incomplete.

When the repository is private, pass `GH_TOKEN` or `GITHUB_TOKEN` through the
wrapper environment so the container can authenticate release downloads.

### Native Linux

Install the same core packages used by `docker/Dockerfile`, including
`gdb-multiarch` for the debugger-launch and debug-host test surface.

Resolve the published installed-toolchain bundle before native Linux configure
or test steps by running:

```bash
bash tools/toolchain/ensure-toolchain-release.sh --repo mundak/ringos-ng
```

For private repositories, export `GH_TOKEN` or `GITHUB_TOKEN` before invoking
the helper so release downloads can authenticate successfully.

Configure and build:

```bash
cmake --preset x64-debug
cmake --build --preset build-x64-debug

cmake --preset arm64-debug
cmake --build --preset build-arm64-debug
```

Run tests:

```bash
ctest --preset x64_emulator_unit
ctest --preset x64_win32_loader_unit
ctest --preset sample_hello_world_x64_native
ctest --preset sample_hello_world_cpp_x64_native
ctest --preset sample_console_service_write_x64_native
ctest --preset sample_hello_world_arm64_native
ctest --preset sample_hello_world_cpp_arm64_native
ctest --preset sample_console_service_write_arm64_native
ctest --preset sample_hello_world_arm64_x64_emulator
ctest --preset sample_hello_world_cpp_arm64_x64_emulator
ctest --preset sample_console_service_write_arm64_x64_emulator
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

## Sample Test Contract

Each sample test script should:

1. Launch the correct QEMU target in headless mode.
2. Capture host-side debug output from the selected launch wrapper.
3. Apply a hard timeout.
4. Assert the expected output appears.
5. Return a non-zero exit code on timeout, early crash, or missing output.

The x64 emulator unit test binary should stay architecture-independent and run
as a host executable under its dedicated CTest preset. Add new instruction coverage
there before expanding the interpreter or introducing a JIT backend.

The x64 Win32 loader unit test binary should also run as a host executable
under its own dedicated CTest preset so PE import-resolution coverage remains
independent from the smoke tests.

Keep raw QEMU command lines inside tests so the same execution path is used by
developers, wrappers, and CTest.

## CI Contract

The current GitHub Actions setup should continue to expose eleven separately
tracked CI workflows:

1. `x64_emulator_unit`
2. `x64_win32_loader_unit`
3. `sample_hello_world_x64_native`
4. `sample_hello_world_cpp_x64_native`
5. `sample_console_service_write_x64_native`
6. `sample_hello_world_arm64_native`
7. `sample_hello_world_cpp_arm64_native`
8. `sample_console_service_write_arm64_native`
9. `sample_hello_world_arm64_x64_emulator`
10. `sample_hello_world_cpp_arm64_x64_emulator`
11. `sample_console_service_write_arm64_x64_emulator`

Each workflow installs the Linux dependency set and builds the matching target.
Most workflows then run exactly one scenario-specific CTest preset.

The `x64_emulator_unit` workflow also runs `bash tests/check-enum-style.sh`
before configure/build so CI rejects `enum class` and legacy SDK numeric
`#define` blocks without needing a dedicated standalone workflow.

Each sample workflow configures the matching architecture, builds only the
single kernel target for that sample-platform lane, and runs exactly one CTest
preset that asserts both platform bring-up and the sample-specific output.

If the dependency stack changes, update `docker/Dockerfile` and
the workflow files under `.github/workflows/` together so local container runs
and CI stay in sync.

The dedicated `toolchain_release` workflow sits outside that CI-gated set and
is responsible for publishing a new release asset when the combined
installed-toolchain manifest ID changes.
