# CI And Testing

This document defines the current verification contract for ringos-ng. The goal
is to keep local development, containerized Windows workflows, and GitHub
Actions aligned around the same Linux toolchain and the same repository-owned
CMake, CTest, and shell entry points.

## Execution Environments

- CI runs on `ubuntu-latest`.
- Windows users are expected to use the Docker-based test wrappers in `tests/`.
- Native Linux is supported for direct shell iteration and GDB-based debugging.
- Shared installed-toolchain consumers should prefer a versioned
	`ringos-toolchain-YYYY.MM.DD.N.tar.xz` archive already present under `build/`, and otherwise download the latest published release asset into that same directory.

The repository should keep one canonical build and test interface even when it
is executed through different shells.

## Source Of Truth

The build and test contract lives in the repository, not in ad hoc CI shell
logic.

Use these layers:

1. Direct top-level CMake configure and build commands are the canonical kernel
	build interface. Pass an explicit build directory,
	`-DCMAKE_TOOLCHAIN_FILE=kernel/toolchains/<arch>.cmake`, and
	`-DRINGOS_TARGET_ARCH=<x64|arm64>`.
2. `CTest` registrations in `cmake/tests/smoke_tests.cmake`,
	`cmake/tests/emulator_tests.cmake`, and `win32/tests/CMakeLists.txt` define
	the host test names.
3. `tests/build-tests.sh` is the canonical sample smoke-test implementation.
	It resolves the published toolchain and SDK, builds the sample, rebuilds the
	matching kernel image, and validates QEMU output.
4. `user/samples/hello_world/*.sh` and `user/samples/hello_world_cpp/*.sh`
	define the active sample lanes by passing lane-specific arguments into
	`tests/build-tests.sh`.
5. `scripts/*.sh` contains shared QEMU run and debug helpers.
6. `tests/docker-*.bat` wraps the Windows container workflow.
7. `.github/workflows/*.yml` should stay thin and invoke repository-owned
	scripts.
8. `tools/toolchain/download-latest-toolchain.sh`,
	`tools/toolchain/build-toolchain.sh`, and `user/sdk/build-sdk.sh` remain the
	published toolchain and SDK entry points.

The workflow file should stay thin. If a command matters for local users, it
should exist in the repository first.

There is currently no top-level `CMakePresets.json` in the repo contract.

## Registered Test Names

Host unit tests:

- `x64_emulator_unit`
- `x64_win32_loader_unit`

Sample smoke tests:

- `sample_hello_world_x64_native`
- `sample_hello_world_cpp_x64_native`
- `sample_hello_world_arm64_native`
- `sample_hello_world_cpp_arm64_native`
- `sample_hello_world_arm64_x64_emulator`
- `sample_hello_world_cpp_arm64_x64_emulator`

Each registered test name maps to exactly one distinct scenario and stops on
the first failure.

## Local Workflows

### Windows Through Docker

Build and run a target in QEMU:

```bat
scripts\docker-run-os-x64.bat
scripts\docker-run-os-arm64.bat
```

Build and smoke-test a target:

```bat
user\samples\hello_world\docker-test-hello-world-x64.bat
user\samples\hello_world\docker-test-hello-world-arm64.bat
user\samples\hello_world\docker-test-hello-world-x64-on-arm64.bat
user\samples\hello_world_cpp\docker-test-hello-world-cpp-x64.bat
user\samples\hello_world_cpp\docker-test-hello-world-cpp-arm64.bat
user\samples\hello_world_cpp\docker-test-hello-world-cpp-x64-on-arm64.bat
```

The current sample-local Windows wrappers live under `user/samples/hello_world/`
and `user/samples/hello_world_cpp/` and call the shared
`tests\docker-run-sample-test.bat` helper.

That helper rebuilds the sample-test image from `tests/tests.Dockerfile`, mounts
the repo-local `build/` directory read-only at `/host-build`, and keeps the
active container build tree on Linux `tmpfs` at `/workspace/build`. If a local
`ringos-toolchain-*.tar.xz` archive is present under the host `build/`
directory, the helper copies it into tmpfs before invoking the sample script.
Otherwise the sample flow downloads the latest published installed-toolchain
bundle into tmpfs first. This keeps `build/toolchain` off the Windows bind
mount while still preferring the newest local archive from the main checkout.

The tmpfs size defaults to `4g`. Override it with
`RINGOS_SAMPLE_BUILD_TMPFS_SIZE` when a lane needs more space.

Outputs under `/workspace/build` are ephemeral in this mode. Use a manual Docker
command when you need to inspect the in-container build tree after the test run.

The toolchain release path uses the same `tools/toolchain/build-toolchain.sh`
entry point that powers the manual `toolchain_release` GitHub Actions job. The
Windows wrapper mounts the repo-local `build` directory so iterative LLVM patch
work can reuse the clone, build directory, and bootstrap install root locally.
The hosted C and bootstrap hosted C++ sample scripts resolve
the published installed-toolchain bundle and compile against the downloaded
compiler configs and sysroot for the selected target. Those sample lanes no
longer depend on repo-local staged bootstrap config files or arbitrary host
compiler paths.

For Windows Docker iteration on native LLVM patches, use:

```bat
tools\toolchain\docker-build-toolchain.bat
```

That wrapper builds the same Docker image used by the `toolchain_release`
workflow, mounts a persistent Docker named volume into `/workspace/build`, and
then runs `tools/toolchain/build-toolchain.sh`. That layout keeps the
installed toolchain bundle under `build/toolchain`, the LLVM clone, the Ninja
build tree, and the bootstrap compiler install under `build/toolchain-build`
on Linux-native Docker storage, so repeated runs can reuse the cached bootstrap
LLVM source, build tree, and install tree when the pinned LLVM ref and patch
set are unchanged instead of restarting from scratch because the Windows host
filesystem is slow.

The wrapper still writes the final versioned `ringos-toolchain-*.tar.xz`
archive into the repo-local `build` directory by default, or into the explicit
path you pass on the command line. Set `RINGOS_TOOLCHAIN_BUILD_VOLUME` to
override the default volume name `ringos-toolchain-build`, and use
`docker volume rm <name>` when you want to discard the cached toolchain build
state.

Before configuring, the sample test flow runs
`tools/toolchain/download-latest-toolchain.sh --archive-dir /workspace/build --install-root /workspace/build/toolchain`
inside tmpfs. If the wrapper copied a local `ringos-toolchain-*.tar.xz` archive
into `/workspace/build`, the helper extracts that archive there. Otherwise it
downloads the latest GitHub Release archive into tmpfs first and then extracts
it. The wrapper still fails immediately if no published release is available or
the archive is incomplete.

When the repository is private, pass `GH_TOKEN` or `GITHUB_TOKEN` through the
wrapper environment so the container can authenticate release downloads.

### Native Linux

Install the same core packages used by `tools/toolchain/Dockerfile`, including
`gdb-multiarch` for the debugger-launch and debug-host test surface.

Resolve the published installed-toolchain bundle before native Linux configure
or test steps by running:

```bash
bash tools/toolchain/download-latest-toolchain.sh --repo mundak/ringos-ng --archive-dir build --install-root build/toolchain
```

For private repositories, export `GH_TOKEN` or `GITHUB_TOKEN` before invoking
the helper so release downloads can authenticate successfully.

Configure and build:

```bash
cmake -S . -B build/x64-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=kernel/toolchains/x64.cmake -DRINGOS_TOOLCHAIN_ROOT=build/toolchain -DRINGOS_TARGET_ARCH=x64 -DRINGOS_ENABLE_TESTING=ON
cmake --build build/x64-debug

cmake -S . -B build/arm64-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=kernel/toolchains/arm64.cmake -DRINGOS_TOOLCHAIN_ROOT=build/toolchain -DRINGOS_TARGET_ARCH=arm64 -DRINGOS_ENABLE_TESTING=ON
cmake --build build/arm64-debug
```

Run tests:

```bash
ctest --test-dir build/x64-debug --output-on-failure
ctest --test-dir build/arm64-debug --output-on-failure
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
as a host executable under its dedicated CTest name. Add new instruction coverage
there before expanding the interpreter or introducing a JIT backend.

The x64 Win32 loader unit test binary should also run as a host executable
under its own dedicated CTest name so PE import-resolution coverage remains
independent from the smoke tests.

Keep raw QEMU command lines inside tests so the same execution path is used by
developers, wrappers, and CTest.

## CI Contract

The current GitHub Actions surface is the set of workflow files under
`.github/workflows/`, not a set of CMake presets.

| Workflow file | Trigger surface | Repo entry point | Current status |
| --- | --- | --- | --- |
| `test-hello-world.yml` | Push, pull request, manual | `user/samples/hello_world/test-hello-world-x64.sh`, `user/samples/hello_world/test-hello-world-arm64.sh`, `user/samples/hello_world/test-hello-world-x64-on-arm64.sh` | Canonical sample CI |
| `test-hello-world-cpp.yml` | Push, pull request, manual | `user/samples/hello_world_cpp/test-hello-world-cpp-x64.sh`, `user/samples/hello_world_cpp/test-hello-world-cpp-arm64.sh`, `user/samples/hello_world_cpp/test-hello-world-cpp-x64-on-arm64.sh` | Canonical sample CI |
| `test-console-service-write.yml` | Manual | Workflow references `user/samples/console_service_write/test-console-service-write.sh`, but that sample path is not present in the repo | Stale workflow; not part of the canonical verification contract until restored |
| `toolchain-release.yml` | Manual | `tools/toolchain/build-toolchain.sh --publish` | Canonical release workflow |
| `sdk-release.yml` | Manual | `user/sdk/build-sdk.sh --publish` | Canonical release workflow |

The active sample workflows build the sample-test container and then delegate
to repo-owned shell entry points. Those shell entry points share the same
`tests/build-tests.sh` implementation for configure, build, toolchain
resolution, QEMU launch, timeout handling, and output assertions.

If the dependency stack changes, update `tools/toolchain/Dockerfile` and
the workflow files under `.github/workflows/` together so local container runs
and CI stay in sync.

The dedicated `toolchain_release` and `sdk_release` workflows sit outside the
push and pull-request CI set and are responsible for publishing new release
assets.
