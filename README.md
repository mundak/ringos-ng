# ringos-ng

A bare-metal operating system targeting the QEMU `virt` AArch64 (ARM64) machine.
This document is the high-level architectural plan: toolchain selection, memory
layout, driver model, kernel structure, build system, and development workflow.

---

## Table of Contents

1. [Development Environment](#1-development-environment)
2. [Toolchain – Where to Get a Compiler](#2-toolchain--where-to-get-a-compiler)
3. [QEMU – Target Machine](#3-qemu--target-machine)
4. [Boot Sequence](#4-boot-sequence)
5. [Memory Layout](#5-memory-layout)
6. [Kernel Architecture](#6-kernel-architecture)
7. [Driver Model](#7-driver-model)
8. [Build System](#8-build-system)
9. [Debugging Workflow](#9-debugging-workflow)
10. [Roadmap](#10-roadmap)

---

## 1. Development Environment

| Tool | Recommended version | Purpose |
|------|--------------------|---------| 
| Cross-compiler | GCC 13+ or Clang/LLVM 17+ | Compile AArch64 bare-metal code |
| QEMU | 8.x | Emulate the `virt` AArch64 board |
| GNU Make + CMake | Make 4.x / CMake 3.25+ | Build orchestration |
| GDB (aarch64) | 13+ | Source-level debugging |
| `dtc` | 1.6+ | Compile Device Tree sources (optional) |
| Python 3 | 3.11+ | Helper scripts, test harness |

All tools are available as packages on major Linux distributions.  macOS users
can use Homebrew; Windows users should use WSL 2.

---

## 2. Toolchain – Where to Get a Compiler

### Option A – Pre-built cross-compiler (recommended for getting started)

**Debian / Ubuntu / Raspberry Pi OS**
```bash
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
```
Produces: `aarch64-linux-gnu-gcc`.  
Even though the suffix says "linux", passing `-ffreestanding -nostdlib -nostartfiles`
produces true bare-metal code with no OS ABI dependencies.

**Fedora / RHEL**
```bash
sudo dnf install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
```

**macOS (Homebrew)**
```bash
brew install aarch64-elf-gcc
```
Produces: `aarch64-elf-gcc` – a proper ELF bare-metal toolchain.

**Arch Linux**
```bash
sudo pacman -S aarch64-linux-gnu-gcc
```

### Option B – Clang / LLVM (single-host, cross-compilation built-in)

Clang is a native cross-compiler; no separate cross-toolchain is needed.

```bash
sudo apt install clang lld llvm
```

Compile with:
```bash
clang --target=aarch64-unknown-none-elf \
      -ffreestanding -nostdlib \
      -march=armv8-a \
      ...
```

`lld` replaces `ld` and understands the same linker scripts, so the rest of the
build system is identical to the GCC path.

### Option C – Build from source via crosstool-NG

Use [crosstool-NG](https://crosstool-ng.github.io/) to build a fully custom
`aarch64-unknown-elf` toolchain:

```bash
ct-ng aarch64-unknown-elf
ct-ng build
```

This gives the most control (specific libc, newlib-nano, no-MMU variant, etc.)
at the cost of a longer build time (~30 min).

### Key compiler flags for bare-metal AArch64

```
-march=armv8-a          # Target ARMv8-A (base for all Cortex-A5x, A7x ...)
-mcpu=cortex-a57        # Match QEMU virt default CPU
-ffreestanding          # No hosted environment assumptions
-nostdlib               # Do not link the standard C library
-nostartfiles           # Do not use the compiler's start files (crt0, etc.)
-fno-builtin            # Disable compiler built-ins that call runtime routines
-mgeneral-regs-only     # (optional) disable SIMD/FP to keep interrupt frames simple
-O2 -g                  # Optimise + keep debug info
```

---

## 3. QEMU – Target Machine

Use the `virt` board – it is a synthetic, well-documented, device-tree-based
machine with no physical counterpart, making it ideal for OS development.

### Install

```bash
sudo apt install qemu-system-aarch64   # Debian/Ubuntu
brew install qemu                      # macOS
```

### Minimal launch command

```bash
qemu-system-aarch64 \
  -M virt \
  -cpu cortex-a57 \
  -m 128M \
  -nographic \
  -kernel build/kernel.elf \
  -serial mon:stdio
```

`-nographic` redirects the first serial port (`UART0`) to the host terminal.
`-serial mon:stdio` lets you switch to the QEMU monitor with `Ctrl-A c`.

### Getting the hardware memory map

```bash
qemu-system-aarch64 -M virt,dumpdtb=virt.dtb -cpu cortex-a57 -m 128M
dtc -I dtb -O dts -o virt.dts virt.dtb
```

The generated Device Tree Source (`virt.dts`) contains every peripheral address,
interrupt number, and clock frequency.  Treat it as the authoritative reference
while writing drivers.

---

## 4. Boot Sequence

The `virt` machine loads the ELF or raw binary kernel directly from the `-kernel`
argument.  No firmware or bootloader is required for early development.

```
Power-on
  │
  ▼
QEMU loads kernel image to 0x4000_0000
  │
  ▼
CPU starts at EL1 (Exception Level 1, equivalent to "kernel mode")
  │
  ▼
_start  (entry.S)
  ├── Set up stack pointer (SP_EL1)
  ├── Zero the BSS section
  ├── Set up the exception vector table (VBAR_EL1)
  ├── Configure SCTLR_EL1 (enable caches, alignment checks, ...)
  ├── (optional) Enable the MMU, set up page tables (TTBR0_EL1)
  └── Branch to kernel_main()  (C entry point)
        │
        ▼
      kernel_main()
        ├── UART init → early console ("Hello, ringos-ng!")
        ├── Interrupt controller init (GIC-v2 or GIC-v3)
        ├── Timer init (ARM Generic Timer)
        ├── Memory allocator init
        └── Scheduler / shell / ...
```

### Exception levels

| EL | Typical use |
|----|------------|
| EL3 | Secure monitor (TrustZone) – QEMU enters here, drops to EL1 |
| EL2 | Hypervisor |
| EL1 | Kernel (ringos-ng runs here) |
| EL0 | Userspace processes |

QEMU's `virt` board drops the CPU to EL1 before the kernel entry point, so
EL3/EL2 set-up can be deferred until hypervisor or TEE support is needed.

---

## 5. Memory Layout

### Physical address map – QEMU `virt` board (128 MB)

| Address range | Size | Device / Purpose |
|---------------|------|-----------------|
| `0x0000_0000` | 128 MiB | Boot ROM / Flash (not normally used) |
| `0x0800_0000` | varies | GIC distributor & CPU interface |
| `0x0900_0000` | 4 KiB | PL011 UART0 (first serial port) |
| `0x0901_0000` | 4 KiB | PL011 UART1 |
| `0x0A00_0000` | 4 KiB | RTC (PL031) |
| `0x0B00_0000` | 4 KiB | VirtIO MMIO region 0 (disk, net, etc.) |
| `0x4000_0000` | RAM size | DRAM – kernel + user processes |

### Kernel virtual address map (after MMU is enabled)

| Virtual range | Mapped to | Contents |
|---------------|-----------|---------|
| `0xFFFF_0000_4000_0000` | `0x4000_0000` | Kernel code + data |
| `0xFFFF_0000_0000_0000` | `0x0000_0000` | MMIO peripheral window |
| `0x0000_0000_xxxx_xxxx` | various | User process memory |

### Linker script skeleton

```ld
/* kernel.ld */
OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS {
    . = 0x40000000;          /* QEMU loads kernel here */

    .text   : { *(.text.boot) *(.text*) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }
}
```

Placing the entry stub (`_start`) in `.text.boot` guarantees it appears first
in the binary, which is where QEMU places the program counter on reset.

---

## 6. Kernel Architecture

ringos-ng follows a **layered ring model**:

```
┌───────────────────────────────────────────────────────┐
│  Ring 3 – User Applications  (EL0)                    │
├───────────────────────────────────────────────────────┤
│  Ring 2 – System Services / Drivers  (EL0, sandboxed) │
├───────────────────────────────────────────────────────┤
│  Ring 1 – Kernel Services  (EL1)                      │
│           scheduler, IPC, VFS, network stack          │
├───────────────────────────────────────────────────────┤
│  Ring 0 – Hardware Abstraction Layer  (EL1)           │
│           interrupt controller, MMU, timers, UART     │
└───────────────────────────────────────────────────────┘
```

* **Ring 0 (HAL)** – the only layer that touches MMIO registers directly.  Every
  peripheral register access goes through a typed `mmio_read32` / `mmio_write32`
  helper (no raw pointer casts in higher rings).
* **Ring 1 (Kernel services)** – a monolithic-ish kernel with well-defined
  subsystem boundaries.  Subsystems communicate through in-kernel function calls,
  not message passing, to keep latency low.
* **Ring 2 (System services)** – privileged user-space daemons (device manager,
  filesystem server) that communicate with Ring 1 via system calls.  A driver
  fault here does not crash the kernel.
* **Ring 3 (Applications)** – fully unprivileged.  Memory is isolated via the
  MMU; system calls are the only way to request OS services.

---

## 7. Driver Model

### Design principles

1. **MMIO-only** – all hardware access is through memory-mapped registers.
   No port I/O (x86-ism).  The HAL exposes typed read/write helpers:
   ```c
   static inline uint32_t mmio_read32(uintptr_t addr) {
       return *(volatile uint32_t *)addr;
   }
   static inline void mmio_write32(uintptr_t addr, uint32_t val) {
       *(volatile uint32_t *)addr = val;
   }
   ```

2. **Interrupt-driven, DMA-capable** – drivers register ISRs with the GIC via
   `irq_register(irq_num, handler, dev_data)`.  Polling is only used during
   early boot (before the GIC is initialised).

3. **Device-Tree driven discovery** – each driver declares the `compatible`
   strings it handles.  The kernel walks the FDT (Flattened Device Tree) at
   boot and calls `driver_probe()` for each match.

4. **Uniform driver interface** – every driver exposes the same lifecycle hooks:

   ```c
   struct driver {
       const char *name;
       const char * const *compatible; /* NULL-terminated */
       int  (*probe) (struct device *dev);
       void (*remove)(struct device *dev);
       int  (*suspend)(struct device *dev);
       int  (*resume) (struct device *dev);
   };
   ```

5. **Class-based I/O interface** – drivers register under a device class
   (`char_device`, `block_device`, `net_device`, ...) and the kernel dispatches
   `read` / `write` / `ioctl` calls through a vtable:

   ```c
   struct char_ops {
       ssize_t (*read) (struct device *, void *buf, size_t n);
       ssize_t (*write)(struct device *, const void *buf, size_t n);
       int     (*ioctl)(struct device *, unsigned cmd, uintptr_t arg);
   };
   ```

### Key drivers for early bring-up (in order)

| Priority | Driver | Hardware | Notes |
|----------|--------|----------|-------|
| 1 | `uart_pl011` | PL011 UART @ `0x09000000` | First output; polling mode initially |
| 2 | `gic_v2` | GIC-400 @ `0x08000000` | Enable interrupts |
| 3 | `arm_timer` | ARM Generic Timer (CP15) | Preemptive scheduler tick |
| 4 | `rtc_pl031` | PL031 RTC @ `0x0A000000` | Wall-clock time |
| 5 | `virtio_blk` | VirtIO block @ `0x0B000000` | Mass storage |
| 6 | `virtio_net` | VirtIO network @ `0x0B001000` | Networking |

### Directory layout for drivers

```
src/
├── hal/
│   ├── mmio.h          # mmio_read32 / mmio_write32
│   ├── gic/            # GIC-v2 and GIC-v3 support
│   └── arm_timer/      # ARM Generic Timer
├── drivers/
│   ├── uart/
│   │   └── pl011.c     # uart_pl011 driver
│   ├── rtc/
│   │   └── pl031.c
│   └── virtio/
│       ├── virtio.c    # VirtIO transport (MMIO)
│       ├── virtio_blk.c
│       └── virtio_net.c
└── kernel/
    ├── irq.c           # IRQ routing / GIC front-end
    ├── device.c        # Device / driver registry
    ├── mm/             # Memory management
    └── sched/          # Scheduler
```

---

## 8. Build System

Use **CMake** as the meta-build tool with **Ninja** as the backend for fast
incremental builds.

### CMakeLists.txt skeleton

```cmake
cmake_minimum_required(VERSION 3.25)
project(ringos-ng ASM C)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Toolchain – override with -DTOOLCHAIN_PREFIX=aarch64-elf-
set(TOOLCHAIN_PREFIX "aarch64-linux-gnu-" CACHE STRING "Cross-compiler prefix")
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)

set(COMMON_FLAGS
    -march=armv8-a -mcpu=cortex-a57 -ffreestanding -nostdlib
    -nostartfiles -fno-builtin -Wall -Wextra -O2 -g)

string(JOIN " " COMMON_FLAGS ${COMMON_FLAGS})

set(CMAKE_C_FLAGS   ${COMMON_FLAGS})
set(CMAKE_ASM_FLAGS ${COMMON_FLAGS})
set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_SOURCE_DIR}/linker/kernel.ld -nostdlib")

add_executable(kernel.elf
    src/boot/entry.S
    src/hal/gic/gic_v2.c
    src/hal/arm_timer/arm_timer.c
    src/drivers/uart/pl011.c
    src/kernel/irq.c
    src/kernel/device.c
    src/kernel/main.c
)

# Convenience targets
add_custom_target(run
    COMMAND qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M
            -nographic -kernel kernel.elf -serial mon:stdio
    DEPENDS kernel.elf
)

add_custom_target(debug
    COMMAND qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M
            -nographic -kernel kernel.elf -serial mon:stdio
            -s -S          # open GDB server on port 1234, pause at start
    DEPENDS kernel.elf
)
```

### Building

```bash
cmake -B build -G Ninja \
      -DTOOLCHAIN_PREFIX=aarch64-linux-gnu-
cmake --build build
```

### Running

```bash
cmake --build build --target run
# or directly:
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M \
  -nographic -kernel build/kernel.elf -serial mon:stdio
```

---

## 9. Debugging Workflow

### GDB + QEMU remote debugging

Terminal 1 – start QEMU with the GDB stub:
```bash
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M \
  -nographic -kernel build/kernel.elf -serial mon:stdio \
  -s -S
# -s  = listen on tcp::1234
# -S  = freeze CPU at startup, wait for GDB
```

Terminal 2 – attach GDB:
```bash
aarch64-linux-gnu-gdb build/kernel.elf
(gdb) target remote :1234
(gdb) break kernel_main
(gdb) continue
```

### QEMU monitor

While QEMU is running, press `Ctrl-A c` to switch to the monitor:
```
(qemu) info registers       # dump all AArch64 registers
(qemu) info mem             # dump MMU page table
(qemu) xp /10i 0x40000000  # disassemble from physical address
(qemu) quit
```

### Semi-hosting (printf without a real UART driver)

For the very earliest boot stages, before the UART driver is up, QEMU supports
ARM semi-hosting: `printf`-style output tunnelled through a special SVC call.

Add `-semihosting` to the QEMU command line and link with `-lrdimon` (Newlib's
semi-hosting support library).  Remove once the real UART driver is working.

---

## 10. Roadmap

| Milestone | Goal |
|-----------|------|
| M0 – Toolchain | Cross-compiler, QEMU, "Hello, world!" over UART |
| M1 – Exceptions | Exception vector table, synchronous/async fault handlers |
| M2 – Interrupts | GIC-v2 init, ARM Generic Timer ISR, preemption |
| M3 – MMU | 4-level page tables, kernel virtual mapping, guard pages |
| M4 – Memory | Physical page allocator, slab/buddy allocator, `kmalloc` |
| M5 – Processes | EL0 processes, `exec`, context switch, round-robin scheduler |
| M6 – System calls | `SVC` dispatch table, basic POSIX syscalls (`read/write/exit`) |
| M7 – VirtIO | Block device driver, simple in-memory filesystem |
| M8 – Networking | VirtIO net driver, minimal TCP/IP stack |
| M9 – Shell | In-kernel serial shell for interactive testing |

---

## References

* [ARMv8-A Architecture Reference Manual](https://developer.arm.com/documentation/ddi0487/latest) – the definitive ISA and system register reference
* [QEMU `virt` board source](https://github.com/qemu/qemu/blob/master/hw/arm/virt.c) – authoritative peripheral list and memory map
* [ARM PL011 Technical Reference Manual](https://developer.arm.com/documentation/ddi0183/latest) – UART register description
* [ARM GIC Architecture Specification](https://developer.arm.com/documentation/ihi0048/latest) – GIC-v2/v3
* [OSDev Wiki – AArch64](https://wiki.osdev.org/AArch64) – community notes on bare-metal AArch64
* [Linaro GCC toolchain downloads](https://releases.linaro.org/components/toolchain/binaries/) – pre-built cross-compilers
* [crosstool-NG](https://crosstool-ng.github.io/) – build a custom bare-metal toolchain
* [LLVM cross-compilation guide](https://llvm.org/docs/HowToCrossCompileLLVM.html)
