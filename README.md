# ringos-ng

ringos-ng is a bare-metal operating system project targeting x64 and arm64. 

## Supported Targets

- x64 in QEMU
- arm64 in QEMU virt

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

These wrappers now look for a versioned toolchain archive
(`ringos-toolchain-YYYY.MM.DD.N.tar.xz`) under the repo-local `build`
directory before configuring the tree. If one is already present, the wrappers
extract that archive into `build/toolchain`. If no archive is present, they
download the latest published release asset into `build/` first and then
extract it into `build/toolchain`. The wrappers mount the repo-local `build`
directory into `/workspace/build` so repeated local Docker runs reuse the same
downloaded archive and extracted bundle instead of rebuilding it.
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

### Native Linux Workflow

Native Linux remains useful for direct shell iteration and GDB-based debugging.
Install the same core dependencies used by the shared container image,
including `gdb-multiarch` for the debug-launch and debug-host test surface:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  curl \
  qemu-system-arm \
  qemu-system-misc \
  qemu-system-x86 \
  gdb-multiarch
```

Resolve the shared toolchain bundle before configuring so the build uses the
published release bundle expected by CI and fails immediately when that bundle
is missing or incomplete. If `build/ringos-toolchain-*.tar.xz` already exists,
the helper extracts that local archive; otherwise it downloads the latest
release archive into `build/` first:

```bash
bash tools/toolchain/download-latest-toolchain.sh --repo mundak/ringos-ng --archive-dir build --install-root build/toolchain
```

For private repositories, export `GH_TOKEN` or `GITHUB_TOKEN` first so the
helper can authenticate release downloads.

Configure and build:

```bash
cmake --preset x64-debug
cmake --build --preset build-x64-debug

cmake --preset arm64-debug
cmake --build --preset build-arm64-debug
```

Build the shared external toolchain package tar.xz archive:

```bash
tools/toolchain/build-toolchain.sh
```

Run the kernels directly:

```bash
scripts/run-x64.sh build/x64-debug/arch/x64/ringos_x64
scripts/run-arm64.sh build/arm64-debug/arch/arm64/ringos_arm64
```

Both direct run wrappers now route boot-time `debug_semihost_log()` output to
the host stream instead of relying on a shared serial console abstraction.

Debug with QEMU's GDB stub:

```bash
scripts/debug-x64.sh build/x64-debug/arch/x64/ringos_x64
gdb-multiarch -ex "target remote :1234" build/x64-debug/arch/x64/ringos_x64.elf64

scripts/debug-arm64.sh build/arm64-debug/arch/arm64/ringos_arm64
gdb-multiarch -ex "target remote :1234" build/arm64-debug/arch/arm64/ringos_arm64
```

Both debug launchers also accept `RINGOS_GDB_PORT` when you need a non-default
stub port. For deterministic testing or custom tooling, they also accept
`RINGOS_QEMU_BIN` to override the QEMU executable path. The x64 launcher also
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

Run smoke tests with CTest:

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

More detail on the local and CI verification contract lives in
[docs/ci-and-testing.md](docs/ci-and-testing.md).

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
