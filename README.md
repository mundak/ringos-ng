# ringos-ng

[![x64 CI](https://github.com/mundak/ringos-ng/actions/workflows/test-hello-world-x64.yml/badge.svg?branch=main)](https://github.com/mundak/ringos-ng/actions/workflows/test-hello-world-x64.yml)
[![arm64 CI](https://github.com/mundak/ringos-ng/actions/workflows/test-hello-world-arm64.yml/badge.svg?branch=main)](https://github.com/mundak/ringos-ng/actions/workflows/test-hello-world-arm64.yml)
[![x64 on arm64 CI](https://github.com/mundak/ringos-ng/actions/workflows/test-hello-world-x64-on-arm64.yml/badge.svg?branch=main)](https://github.com/mundak/ringos-ng/actions/workflows/test-hello-world-x64-on-arm64.yml)

ringos-ng is a bare-metal operating system project targeting x64 and arm64. 

## Supported Targets

- x64 in QEMU
- arm64 in QEMU virt

## Build Layer Boundaries

The repository is intentionally split into separate build-consumer layers. Keep
those boundaries strict.

### 1. Toolchain Archive Builder

Everything under `tools/toolchain/` exists only to build and publish the shared
toolchain archive.

It builds LLVM, packages clang/lld/binutils, and assemble sysroots.

The output of that layer is a versioned archive such as
`build/ringos-toolchain-YYYY.MM.DD.N.tar.xz`, which extracts into
`build/toolchain/`.

### 2. Kernel Build Consumer

Kernel builds consumes only the extracted toolchain archive under
`build/toolchain/`.

- Kernel configuration and linking must not read files directly from
  `tools/toolchain/`.
- If the kernel needs a CMake toolchain file or other build metadata, that file
  must be shipped inside the extracted toolchain bundle.
- The kernel build may consume the bundled compiler binaries, linker, objcopy,
  sysroots, and bundled CMake toolchain files.

In practice, that means the kernel-side build graph under `kernel/`, `arch/`,
and the top-level CMake entry points should treat `build/toolchain/` as the
only toolchain input.

### 3. Sample Projects

Projects under `user/samples/` must stay independent from both the kernel build
rules and the toolchain-builder implementation.

- Samples should know only about the SDK surface under `user/` and the
  extracted toolchain bundle under `build/toolchain/`.
- Samples must not read files directly from `tools/toolchain/`.
- Samples must not require knowledge of kernel internals just to build their
  own `.exe` payloads.

The client-facing CMake configuration that sample apps use belongs in the
shipped bundle files such as `build/toolchain/cmake/ringos-toolchain.cmake`
and its per-target variants, not in repo-local `tools/toolchain/` sources.

### Temporary Workaround: Embedded App Images

The kernel cannot load files from a filesystem yet. Until that exists, sample
tests may temporarily combine two steps:

1. Build the sample executable against the extracted bundle in `build/toolchain`.
2. Rebuild a kernel image that embeds that executable as an app image and boot
   it under QEMU.

Even in that temporary path, the sample test flow must still consume only the
extracted toolchain bundle and the public SDK surface under `user/`. It should
not reach back into `tools/toolchain/` during configure or build steps.

## Development Workflow

### Recommended on Windows: Docker

The repository includes Windows batch wrappers that build the shared toolchain
image from [tools/toolchain/Dockerfile](tools/toolchain/Dockerfile) and run the
full build inside that container.

Requirements:

- Docker Desktop or an equivalent Docker engine

Run a target in QEMU from Windows:

```bat
scripts\docker-run-os-x64.bat
scripts\docker-run-os-arm64.bat
```

Build and smoke-test a target from Windows:

```bat
user\samples\hello_world\docker-test-hello-world-x64.bat
user\samples\hello_world\docker-test-hello-world-arm64.bat
user\samples\hello_world\docker-test-hello-world-x64-on-arm64.bat
```

The current sample-local Windows wrappers live under `user/samples/hello_world/`
and delegate to `tests\docker-run-sample-test.bat`.

That shared wrapper now keeps `/workspace/build` on Linux `tmpfs` and mounts the
repo-local `build/` directory read-only at `/host-build`. If a versioned
`ringos-toolchain-YYYY.MM.DD.N.tar.xz` archive is already present under the
host `build/` directory, the wrapper copies it into tmpfs before running the
sample test script. If no local archive is present, the test flow downloads the
latest published release into tmpfs first. This avoids unpacking
`build/toolchain` onto the Windows bind mount.

The tmpfs size defaults to `4g`. Override it with
`RINGOS_SAMPLE_BUILD_TMPFS_SIZE` if a local sample lane needs more space.

Outputs under `/workspace/build` are ephemeral in this mode. Use a manual Docker
invocation if you need to inspect the in-container build tree after the test.
If the repository is private, set `GH_TOKEN` or `GITHUB_TOKEN` in the host
environment so the container can authenticate release downloads instead of
falling back to a local rebuild.

Build the shared external toolchain package tar.xz archive from Windows:

```bat
tools\toolchain\docker-build-toolchain.bat
```

That wrapper now builds the same Docker image and runs the same
`tools/toolchain/run-toolchain-release.sh` entry point as the manual
`toolchain_release` GitHub Actions job. The local distinction is that it mounts
the repo-local `build` directory into `/workspace/build` so the bootstrap LLVM
state persists under `build/toolchain-build` and the resulting archive is
written to local disk instead of publishing.

For direct shell invocation without the Windows wrapper, run
`tools/toolchain/run-toolchain-release.sh`.

If you want to invoke the container manually instead of using the wrappers:

```powershell
docker build -f tools/toolchain/Dockerfile -t ringos-ci .
docker run --rm ringos-ci bash -lc "cmake --preset x64-debug && cmake --build --preset build-x64-debug && ctest --preset x64_emulator_unit && ctest --preset x64_win32_loader_unit && ctest --preset sample_hello_world_x64_native && ctest --preset sample_hello_world_cpp_x64_native && ctest --preset sample_console_service_write_x64_native"
docker run --rm ringos-ci bash -lc "cmake --preset arm64-debug && cmake --build --preset build-arm64-debug && ctest --preset sample_hello_world_arm64_native && ctest --preset sample_hello_world_cpp_arm64_native && ctest --preset sample_console_service_write_arm64_native && ctest --preset sample_hello_world_arm64_x64_emulator && ctest --preset sample_hello_world_cpp_arm64_x64_emulator && ctest --preset sample_console_service_write_arm64_x64_emulator"
```

bash tools/toolchain/download-latest-toolchain.sh --repo mundak/ringos-ng --archive-dir build --install-root build/toolchain
accepts `RINGOS_DEBUGCON` to redirect the port `0xe9` debug console sink.

Inside the kernel, include `debug.h` when you want lightweight trace markers or
an explicit debugger trap:

```cpp
debug_semihost_log("visible on the host-side debug channel");
debug_semihost_log("reached scheduler bring-up");
debug_break("inspect scheduler state");
```

`debug_semihost_log` is meaningful under both debug launchers. On arm64,
`scripts/debug-arm64.sh` enables semihosting and routes it to the attached GDB
session. On x64, `scripts/debug-x64.sh` routes port `0xe9` through QEMU's x86
debug console.

Run smoke tests with CTest.

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

## Shared Toolchain Release

The installed-toolchain flow publishes manual GitHub releases tagged as
`ringos-toolchain-YYYY.MM.DD.N`, where `N` increments when multiple toolchain
releases are created on the same UTC day. Each release carries a download
asset named after that tag, such as `ringos-toolchain-2026.05.05.01.tar.xz`,
and the extracted archive records
that published version in `share/ringos/toolchain-version.txt` plus the
per-target toolchain manifests. The dedicated `toolchain_release` workflow is
manual-only: it computes the next date-based version, builds the shared x64 and
arm64 bundle, and publishes that bundle to GitHub Releases. Test and Docker
wrapper flows only download or verify the latest published bundle; they do not
build toolchains on demand.

## User-Space Samples

The user-facing sample entry points under `user/samples/` should consume only
the published installed-toolchain bundle and its bundled sysroots.

They should not depend on repo-local `tools/toolchain/` implementation details,
and they should not depend on kernel-private headers or build rules just to
produce their own user executable payload.

## Architecture Overview

The current codebase implements the earliest boot, debug-host logging, and
shared kernel boundary. The long-term design direction is still a microkernel:
keep the privileged kernel as small as possible and push drivers and
higher-level services into isolated user-space components.

### Design Direction

- Isolation between kernel, drivers, and services
- Minimal privileged surface area
- Thin architecture-specific layers under `arch/`
- Shared policy-neutral mechanisms in `kernel/`

### Shared Kernel Boundary

The shared code in `kernel/` currently provides:

- `boot_info` as the boot handoff structure populated by each architecture
  before calling `kernel_main`
- `kernel_main` as the shared C++ convergence point
- `panic` as the minimal panic path that reports failure through the debug log
  and halts
- `debug_semihost_log` and `debug_break` as the shared debugger-oriented trace
  and trap hooks

The boot contract assumes:

- a valid stack
- cleared BSS
- the intended CPU mode for the target architecture
- the selected launcher has configured the host-side debug log sink

### Architecture-Specific Responsibilities

Architecture-dependent code lives under `arch/<target>/` and is responsible
for:

- early boot entry and CPU-mode setup
- stack setup and final handoff into `kernel_main`
- linker layout
- target-specific debug and emulator behavior

In the current tree that means:

- `arch/x64/` contains the Multiboot-friendly startup path, debugcon-backed
  debug transport, linker script, and x64 entry handoff
- `arch/arm64/` contains the QEMU virt startup path, semihost-backed debug
  transport, linker script, and arm64 entry handoff

### Microkernel Direction

Most kernel subsystems described here are architectural targets rather than
implemented features. The intended kernel responsibilities remain:

| Responsibility | Description |
| --- | --- |
| Address-space management | Create, destroy, and switch page tables for processes. |
| Process and thread management | Create, schedule, suspend, and terminate execution contexts. |
| IPC primitives | Deliver messages between isolated components. |
| Interrupt routing | Receive hardware interrupts and forward them to the correct service. |
| Capability and access control | Track which processes may access which kernel objects. |

The kernel should not absorb device drivers, filesystem logic, protocol stacks,
or application-level policy.

### Drivers, Services, and IPC

The intended execution model is:

- drivers run as isolated user-space processes
- services communicate through kernel-mediated IPC
- shared memory is granted explicitly rather than assumed implicitly
- service discovery is handled by a dedicated name-service process

The planned RPC flow is synchronous:

1. A client prepares a request message.
2. The client traps into the kernel.
3. The kernel validates the request and routes it to the target service.
4. The server prepares a reply.
5. The kernel returns the reply to the client and resumes it.

### User-Space ABI and Toolchain Direction

The first hosted software stack should be layered deliberately instead of
making libc the kernel ABI.

- The kernel boundary should be a small C SDK that exposes raw syscall entry
  points, kernel object handles, message structures, startup conventions, and
  basic error reporting.
- libc should sit on top of that SDK and translate ISO C or POSIX-like APIs
  into syscalls plus user-space service calls.
- Architecture-specific trap details should stay behind thin per-target SDK
  thunks so higher layers share one semantic contract even though x64 and arm64
  use different calling conventions.
- The first user programs should link against the SDK directly before libc is
  introduced, so the syscall ABI and process startup path can be validated in
  isolation.

This keeps the ABI surface small, makes libc replaceable, and matches the
microkernel goal of pushing policy into user space.

The expected bootstrap order is:

1. Define the syscall ABI and process startup contract.
2. Build a target C SDK on top of that contract.
3. Build libc on top of the SDK and user-space services.
4. Build a Clang-targeted sysroot that uses that libc.

The Stage 0 baseline for that work lives in [docs/user-space-abi.md](docs/user-space-abi.md).

The current Stage 7 bootstrap packages the SDK sysroot under
`build/<preset>/sysroot/<triple>` and exposes a `ringos_sdk_sysroot` build
target plus `stage-x64-sdk-sysroot` and `stage-arm64-sdk-sysroot` build
presets. Each packaged sysroot now carries bootstrap Clang config files, a
`crt0.obj` startup object, a minimal hosted C library archive implemented in
C++ behind a C ABI, a bootstrap compiler-rt builtins archive, and toolchain
metadata under `share/ringos/` so external build tooling can discover the
provisional target triple and runtime layout.

Stage 8 now freezes the first ringos-specific compiler contract in
[docs/stage8-clang-bringup.md](docs/stage8-clang-bringup.md). The planned
ringos-aware compiler surface adopts `x86_64-unknown-ringos-msvc` and
`aarch64-unknown-ringos-msvc`, keeps the current PE or COFF bootstrap path, and
replaces bootstrap config-file discovery with compiler-owned target and sysroot
defaults.

In the current bootstrap cut, the libc archive is intentionally small: console
output and basic string routines are present, the malloc family is stubbed out
until later work, and the arm64-hosted x64-emulator smoke payload still uses a
minimal assembly guest while the x64 emulator grows beyond its current
instruction subset.

Clang is only one part of hosted C++ support. After libc exists, the toolchain
bring-up also needs the target runtime pieces that Clang expects, especially
compiler-rt and the usual C++ runtime stack such as libunwind, libc++abi, and
libc++.

The staged implementation sketch for that path lives in
[HelloWorld.md](HelloWorld.md).

### Near-Term Priorities

The next major areas of work are:

- expanding boot information passed into the shared kernel
- MMU and virtual-memory design
- interrupt and timer support
- user-space process model, syscall ABI, and IPC implementation
- target process startup ABI, CRT objects, and sysroot layout
- C SDK bring-up before libc
- libc bring-up on top of the SDK and user-space services
- Clang, compiler-rt, and C++ runtime bring-up against the target sysroot
- device discovery and hardware abstraction
- storage, filesystem, and networking services
