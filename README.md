# ringos-ng

This repository is planned as a bare-metal operating system project targeting multiple CPU architectures.

## Current Scope

The current approved target is milestone one only:

- Boot a minimal kernel in QEMU on x64.
- Boot a minimal kernel in QEMU virt on arm64.
- Reach a shared C++ kernel entry path on both targets.
- Print hello world over serial on both targets.
- Build and run from WSL2 on Windows.

## Milestone One Plan

The detailed implementation plan is documented in [docs/milestone-one-execution-plan.md](docs/milestone-one-execution-plan.md).
The recommended CI and local testing structure is documented in [docs/ci-and-testing.md](docs/ci-and-testing.md).

That document is the current source of truth for:

- scope and non-goals
- execution phases
- architecture boundaries
- verification gates
- handoff expectations for the next implementation agent

## Milestone One Non-Goals

These are explicitly out of scope for the first milestone:

- SMP
- interrupts
- timers
- scheduler
- userspace
- filesystem or storage
- framebuffer output
- real hardware support
- advanced MMU work beyond what is strictly needed for x64 kernel entry

## Development Environment

- Host OS: Windows
- Primary build and debug environment: WSL2
- Language after early bootstrap: C++
- Expected early-bootstrap language: assembly
- Initial emulation targets: QEMU x64 and QEMU arm64 virt

## Repository Structure

```text
.
├── CMakeLists.txt          # Top-level CMake project
├── CMakePresets.json       # Configure, build, and test presets for both targets
├── cmake/
│   ├── toolchains/
│   │   ├── x64.cmake       # x64 bare-metal toolchain (x86_64-linux-gnu)
│   │   └── arm64.cmake     # arm64 bare-metal toolchain (aarch64-linux-gnu)
│   └── tests/
│       └── smoke_tests.cmake
├── kernel/                 # Shared kernel code (populated from Phase 2)
├── arch/
│   ├── x64/                # x64-specific boot and hardware code
│   └── arm64/              # arm64-specific boot and hardware code
├── scripts/
│   ├── run-x64.sh          # Launch x64 QEMU with serial on stdout
│   ├── run-arm64.sh        # Launch arm64 QEMU virt with serial on stdout
│   ├── debug-x64.sh        # Launch x64 QEMU with GDB stub, paused at startup
│   ├── debug-arm64.sh      # Launch arm64 QEMU virt with GDB stub, paused
│   ├── test-smoke-x64.sh   # x64 smoke test (asserts expected serial output)
│   └── test-smoke-arm64.sh # arm64 smoke test (asserts expected serial output)
└── docs/
    ├── milestone-one-execution-plan.md
    └── ci-and-testing.md
```

## Prerequisites

Install the following tools in WSL2 (Ubuntu):

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake \
  ninja-build \
  gcc \
  g++ \
  gcc-aarch64-linux-gnu \
  g++-aarch64-linux-gnu \
  binutils-aarch64-linux-gnu \
  gcc-x86-64-linux-gnu \
  g++-x86-64-linux-gnu \
  binutils-x86-64-linux-gnu \
  qemu-system-arm \
  qemu-system-misc \
  qemu-system-x86 \
  gdb-multiarch
```

## Build Commands

### x64

```bash
cmake --preset x64-debug
cmake --build --preset build-x64-debug
```

### arm64

```bash
cmake --preset arm64-debug
cmake --build --preset build-arm64-debug
```

## Run Commands

```bash
# x64
scripts/run-x64.sh build/x64-debug/arch/x64/ringos_x64

# arm64
scripts/run-arm64.sh build/arm64-debug/arch/arm64/ringos_arm64
```

## Debug Commands

```bash
# x64 — launches QEMU with GDB stub, paused at startup
scripts/debug-x64.sh build/x64-debug/arch/x64/ringos_x64

# In a separate terminal:
gdb-multiarch -ex "target remote :1234" build/x64-debug/arch/x64/ringos_x64

# arm64 — same pattern
scripts/debug-arm64.sh build/arm64-debug/arch/arm64/ringos_arm64
gdb-multiarch -ex "target remote :1234" build/arm64-debug/arch/arm64/ringos_arm64
```

## Smoke Tests

```bash
ctest --preset test-x64-debug
ctest --preset test-arm64-debug
```

## Status

Phase 3 (x64 Bring-Up) is complete. The x64 target boots in QEMU, prints its
banner and hello world over COM1 serial, and the smoke test passes.

The following shared services are implemented in `kernel/`:

- `boot_info` — boot handoff structure populated by each architecture before
  calling `kernel_main`; carries the architecture ID.
- `kernel_main` — the shared C++ entry point that both architectures must call
  after completing their startup sequence.
- `console_write` — serial console write service; a weak no-op stub in the
  kernel that each architecture replaces with its real driver.
- `panic` — halts the system with an error message written to the console.
- `kprint` — tiny fixed-text formatting utility that builds on `console_write`.

x64-specific components in `arch/x64/`:

- `boot.S` — Multiboot header, 32-bit to 64-bit long mode transition, BSS
  clear, stack setup, and call to the C++ entry.
- `serial.cpp` — COM1 serial driver providing the strong `console_write`
  override via port I/O.
- `entry.cpp` — C++ entry point that initialises serial, populates `boot_info`,
  and calls `kernel_main`.

The build produces a 64-bit ELF (saved as `ringos_x64.elf64` for GDB) and
converts it to a 32-bit ELF (`ringos_x64`) that QEMU's Multiboot loader
accepts.

arm64 bring-up (Phase 4) can proceed independently.
