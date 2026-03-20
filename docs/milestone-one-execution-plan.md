# Milestone One Execution Plan

Deliver one narrow milestone: build a minimal bare-metal kernel foundation that boots in QEMU on x64 and arm64, reaches a shared C++ entry path, and prints hello world over serial on both targets from the same repository using WSL2.

The recommended execution order is:

1. Freeze the shared contract first.
2. Build the host workflow and repository skeleton.
3. Bring up x64 and arm64 in parallel.
4. Converge both into the same shared startup path.
5. Lock the milestone with smoke tests and documentation.

## Phase 0: Scope Lock

1. Support only two targets:
   - x64 in QEMU
   - arm64 on QEMU virt
2. Define milestone-one success as:
   - both targets build in WSL2
   - both boot in QEMU
   - both initialize serial output
   - both reach the same shared kernel entry
   - both print the expected hello world string
3. Keep these out of scope:
   - SMP
   - interrupts
   - timers
   - scheduler
   - userspace
   - filesystem
   - framebuffer
   - real hardware
   - advanced virtual memory beyond what x64 needs to enter 64-bit kernel code

### Exit Criteria

1. The milestone is small enough that every task either directly contributes to booting or is removed.

## Phase 1: Host Workflow And Build Skeleton

1. Standardize the toolchain in WSL2:
   - compiler
   - assembler
   - linker
   - objcopy
   - QEMU for both targets
   - gdb-multiarch
2. Use one top-level CMake project with separate architecture presets or toolchain selections.
3. Define stable commands early:
   - configure-x64
   - build-x64
   - run-x64
   - debug-x64
   - configure-arm64
   - build-arm64
   - run-arm64
   - debug-arm64
4. Create the initial repository structure for:
   - shared kernel code
   - x64 code
   - arm64 code
   - scripts
   - documentation
5. Start documentation in the repository entry points so the workflow is explicit before code appears.

### Exit Criteria

1. A clean WSL2 checkout can configure placeholder x64 and arm64 targets successfully.

## Phase 2: Shared Kernel Contract

1. Freeze one boot handoff structure for both architectures.
2. Keep the boot info minimal:
   - architecture id
   - placeholder for future memory information
   - reserved expansion fields
3. Freeze one shared entrypoint signature that both architectures must call.
4. Make the preconditions explicit:
   - stack is valid
   - BSS is cleared
   - control has reached the intended CPU mode
   - console is ready or can be initialized from known constants
5. Freeze the shared runtime restrictions:
   - no exceptions
   - no RTTI
   - no libc dependency
   - no dynamic allocation
   - no implicit global constructor reliance
6. Define the shared services for milestone one:
   - console write
   - panic
   - tiny fixed-text formatting support

### Exit Criteria

1. The ABI boundary and startup contract are documented and considered stable.
2. This is the point where x64 and arm64 work can proceed in parallel.

## Phase 3: x64 Bring-Up

1. Use a Multiboot2-friendly loader path under QEMU.
2. Keep the x64 startup path minimal:
   - receive control
   - establish the required 64-bit execution conditions
   - initialize a stack
   - clear BSS if needed
   - jump to the shared entrypoint
3. Implement only COM1 serial output through port I/O.
4. Add the x64 linker and image rules needed for a QEMU-runnable artifact with symbols preserved.
5. Add immediate smoke verification:
   - boot in QEMU
   - capture serial stdio
   - check for x64 banner
   - check for hello world

### Exit Criteria

1. x64 boots reproducibly in QEMU.
2. x64 prints the banner and hello world.
3. QEMU can pause at startup for GDB attach.

## Phase 4: arm64 Bring-Up

1. Use QEMU virt with direct kernel loading.
2. Keep the arm64 startup path minimal:
   - accept QEMU entry conditions
   - initialize a stack
   - clear BSS
   - preserve only the minimal boot state required
   - branch to the shared entrypoint
3. Implement only PL011 UART output through MMIO.
4. Add the arm64 linker and image rules needed for a QEMU-loadable artifact with symbols preserved.
5. Add immediate smoke verification:
   - boot under QEMU virt
   - capture serial stdio
   - check for arm64 banner
   - check for hello world

### Exit Criteria

1. arm64 boots reproducibly in QEMU virt.
2. arm64 prints the banner and hello world.
3. QEMU can pause at startup for GDB attach.

## Phase 5: Shared Startup Convergence

1. Remove any temporary architecture-local print logic.
2. Make the shared startup path responsible for:
   - common banner format
   - hello world text
   - panic behavior
3. Normalize handoff data enough that shared code only branches on architecture identity where truly necessary.
4. Review boundaries and move any accidental low-level ISA or device logic out of shared code.

### Exit Criteria

1. Both targets reach the same shared C++ entry routine.
2. Architecture differences are limited to startup, linker layout, and serial backend.

## Phase 6: Repeatable Verification And Developer Ergonomics

1. Add stable run and debug scripts or build targets so raw QEMU commands are not tribal knowledge.
2. Add automated smoke tests for both targets with timeouts and serial-output assertions.
3. Document a minimal GDB workflow using QEMU's GDB stub and gdb-multiarch.
4. If CI is added this early, keep it minimal:
   - build both targets
   - run both smoke tests
   - use Linux only

### Exit Criteria

1. A fresh contributor can build, run, and smoke-test both targets from WSL2 using documented commands.

## Phase 7: Milestone Closeout

1. Update the repository documentation with:
   - supported targets
   - non-goals
   - prerequisites
   - build commands
   - run commands
   - debug commands
   - architecture boundary summary
2. Record why the boot strategy is asymmetric:
   - it is a milestone-one optimization, not a permanent architectural commitment
3. Capture the post-milestone backlog only after the current milestone is verifiably done:
   - boot info expansion
   - memory map normalization
   - MMU design
   - exception handling
   - timer abstraction
   - interrupt support

### Exit Criteria

1. The repository clearly explains what milestone one is and how to reproduce it.
2. There is no ambiguity about what was intentionally deferred.

## Critical Sequencing

1. Phase 0 blocks everything.
2. Phase 1 depends on Phase 0.
3. Phase 2 depends on Phase 1.
4. Phase 3 and Phase 4 can run in parallel after Phase 2 is frozen.
5. Phase 5 depends on both Phase 3 and Phase 4.
6. Phase 6 depends on Phase 5.
7. Phase 7 depends on Phase 6.

## Verification Gate For Milestone Completion

1. Clean WSL2 checkout builds x64.
2. Clean WSL2 checkout builds arm64.
3. x64 QEMU boot prints expected serial output.
4. arm64 QEMU virt boot prints expected serial output.
5. Both targets support paused startup and GDB attach.
6. Shared kernel code contains no direct port I/O, MMIO, or register manipulation.
7. The documentation is sufficient for a new contributor to reproduce the result.

## Decisions

1. Milestone one remains intentionally asymmetric at boot: x64 uses a Multiboot2-friendly loader path, arm64 uses QEMU virt direct kernel loading.
2. The convergence point is the shared C++ kernel entry, not the loader mechanism.
3. Assembly is limited to the shortest possible architecture-specific startup path.
4. Serial output is the only required I/O path in milestone one.
5. Documentation and smoke tests are part of the milestone, not cleanup work after the milestone.

## Next Agent Starting Point

1. Start with phase 1.
2. Create the repository skeleton and top-level build configuration.
3. Do not begin architecture-specific bring-up until the shared entry contract is written down in code and documentation.
4. Keep milestone-one scope frozen unless a hard technical blocker forces a revision.