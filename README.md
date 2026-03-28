# ringos-ng

ringos-ng is a bare-metal operating system project targeting x64 and arm64. The
current repository boots both targets in QEMU, converges into shared C++ kernel
code, and verifies the bring-up path with smoke tests for each architecture.

## Current State

- x64 boots in QEMU, initializes COM1 serial, reaches the shared C++ kernel
  entry point, and prints the expected banner and hello-world output.
- arm64 boots in QEMU virt, initializes the PL011 UART, reaches the same shared
  C++ kernel entry point, and prints the expected banner and hello-world output.
- Shared kernel code owns the boot handoff contract, common output path, and
  panic handling.
- CI builds and smoke-tests both supported targets.

## Supported Targets

- x64 in QEMU
- arm64 in QEMU virt

## Not Implemented Yet

- SMP
- interrupts and timers
- scheduler and process model
- userspace services and drivers
- filesystem or storage
- framebuffer output
- real hardware support
- advanced MMU work beyond what is currently required for boot and shared entry

## Development Workflow

### Recommended on Windows: Docker

The repository includes Windows batch wrappers that build the shared toolchain
image from [docker/Dockerfile](docker/Dockerfile) and run the full build inside
that container.

Requirements:

- Docker Desktop or an equivalent Docker engine

Run a target in QEMU from Windows:

```bat
scripts\docker-run-os-x64.bat
scripts\docker-run-os-arm64.bat
```

Build and smoke-test a target from Windows:

```bat
scripts\docker-test-x64.bat
scripts\docker-test-arm64.bat
```

If you want to invoke the container manually instead of using the wrappers:

```powershell
docker build -f docker/Dockerfile -t ringos-ci .
docker run --rm ringos-ci bash -lc "cmake --preset x64-ci && cmake --build --preset build-x64-ci && ctest --preset test-x64-ci"
docker run --rm ringos-ci bash -lc "cmake --preset arm64-ci && cmake --build --preset build-arm64-ci && ctest --preset test-arm64-ci"
```

### Native Linux Workflow

Native Linux remains useful for direct shell iteration and GDB-based debugging.
Install the same core dependencies used by the shared container image,
including `gdb-multiarch` for the arm64 debug and semihosting test surface:

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake \
  ninja-build \
  clang \
  lld \
  llvm \
  qemu-system-arm \
  qemu-system-misc \
  qemu-system-x86 \
  gdb-multiarch
```

Configure and build:

```bash
cmake --preset x64-debug
cmake --build --preset build-x64-debug

cmake --preset arm64-debug
cmake --build --preset build-arm64-debug
```

Run the kernels directly:

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

Both debug launchers also accept `RINGOS_GDB_PORT` when you need a non-default
stub port. For deterministic testing or custom tooling, they also accept
`RINGOS_QEMU_BIN` to override the QEMU executable path.

Inside the kernel, include `debug.h` when you want lightweight trace markers or
an explicit debugger trap:

```cpp
debug_log("reached scheduler bring-up");
debug_semihost_log("visible in the arm64 GDB session via semihosting");
debug_break("inspect scheduler state");
```

`debug_semihost_log` is currently meaningful on arm64 when launched through
`scripts/debug-arm64.sh`, which enables semihosting and routes it to the
attached GDB session. `debug_log` continues to write to the serial console on
all targets.

Run smoke tests with CTest:

```bash
ctest --preset test-x64-debug
ctest --preset test-arm64-debug
```

More detail on the local and CI verification contract lives in
[docs/ci-and-testing.md](docs/ci-and-testing.md).

## Repository Layout

```text
.
├── CMakeLists.txt          # Top-level CMake project
├── CMakePresets.json       # Configure, build, and test presets for both targets
├── arch/
│   ├── arm64/              # arm64-specific boot, UART, linker, and entry code
│   └── x64/                # x64-specific boot, serial, linker, and entry code
├── cmake/
│   ├── tests/
│   │   └── smoke_tests.cmake
│   └── toolchains/
│       ├── arm64.cmake
│       └── x64.cmake
├── docker/
│   └── Dockerfile          # Shared Linux toolchain image for local container runs
├── kernel/                 # Architecture-neutral kernel code and public headers
├── scripts/
│   ├── debug-arm64.sh
│   ├── debug-x64.sh
│   ├── docker-run-os-arm64.bat
│   ├── docker-run-os-x64.bat
│   ├── docker-test-arm64.bat
│   ├── docker-test-x64.bat
│   ├── run-arm64.sh
│   ├── run-x64.sh
│   ├── test-smoke-arm64.sh
│   └── test-smoke-x64.sh
└── docs/
    ├── ci-and-testing.md
    └── contributing.md
```

## Architecture Overview

The current codebase implements the earliest boot, console, and shared kernel
boundary. The long-term design direction is still a microkernel: keep the
privileged kernel as small as possible and push drivers and higher-level
services into isolated user-space components.

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
- `console_write` as the common console abstraction with architecture-local
  serial backends overriding the weak default
- `panic` as the minimal panic path that writes to the console and halts
- `kprint` as the current fixed-text formatting helper
- `debug_log`, `debug_semihost_log`, and `debug_break` as the shared
  debugger-oriented trace and trap hooks

The boot contract assumes:

- a valid stack
- cleared BSS
- the intended CPU mode for the target architecture
- a serial console that is ready to accept writes

### Architecture-Specific Responsibilities

Architecture-dependent code lives under `arch/<target>/` and is responsible
for:

- early boot entry and CPU-mode setup
- stack setup and final handoff into `kernel_main`
- linker layout
- serial console backend implementation
- target-specific debug and emulator behavior

In the current tree that means:

- `arch/x64/` contains the Multiboot-friendly startup path, COM1 serial driver,
  linker script, and x64 entry handoff
- `arch/arm64/` contains the QEMU virt startup path, PL011 UART driver, linker
  script, and arm64 entry handoff

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

### Near-Term Priorities

The next major areas of work are:

- expanding boot information passed into the shared kernel
- MMU and virtual-memory design
- interrupt and timer support
- user-space process model and IPC implementation
- device discovery and hardware abstraction
- storage, filesystem, and networking services
