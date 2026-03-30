# Hello World Bring-Up Plan

This file sketches a small-stage path from the current boot-only kernel to the
first hosted user-space programs.

The intended layering is:

1. kernel syscalls and process startup ABI
2. cross-architecture user-mode execution and personality plumbing
3. a thin C SDK that talks to the kernel directly
4. libc implemented on top of that SDK
5. Clang plus the target runtime libraries on top of the resulting sysroot

Building Clang against the target libc is necessary, but it is not sufficient
for full hosted C++ support by itself. The plan therefore includes the runtime
pieces that normally sit next to Clang: compiler-rt, libunwind, libc++abi, and
libc++.

## Stage 0: Freeze the Execution Model

Goal:

- document the initial process model, handle model, and IPC model before code
  is written

Deliverables:

- a short syscall design note with register usage for x64 and arm64
- a first pass at kernel object types, error codes, and capability rules
- a decision on the first supported user image format and target triples

Exit criteria:

- one written ABI document exists and both architectures can implement it

## Stage 1: Kernel Primitives for User Space

Goal:

- make it possible to enter and leave the kernel safely from user mode

Deliverables:

- per-architecture trap entry and return paths
- kernel support for user address spaces, kernel stacks, and thread contexts
- minimal kernel objects for process, thread, channel, and shared memory

Exit criteria:

- the kernel can create a user task and return to it after a simple syscall

## Stage 2: Process Startup Contract

Goal:

- define how a user program starts before libc exists

Deliverables:

- a loader for statically linked user images
- an x64 PE64 proof image built with clang's Windows target and no system libraries
- an initial stack layout for argc, argv, envp, and auxiliary data if present
- a tiny crt0-style entry path that calls into a user-defined main function

Exit criteria:

- a trivial user image can start, run, and exit without libc

### Stage 2A: Cross-Architecture Proof Path

Goal:

- prove that the Stage 2 x64 proof image can execute on arm64 through the
  future emulation path before Windows compatibility work begins

Deliverables:

- a generalized process model that can describe host architecture, guest
  architecture, process personality, and execution backend
- a canonical service-request or syscall frame that does not depend on one
  native trap instruction
- an x64-on-arm64 proof backend that can execute the existing minimal x64 user
  image

Exit criteria:

- the same minimal x64 proof image can run on x64 natively and on arm64 through
  the emulation backend

### Stage 2B: Windows Personality Proof Path

Goal:

- prove the first user-space Windows compatibility path without involving ISA
  translation yet

Deliverables:

- a user-space Windows-compatible loader for a minimal x64 PE executable
- the first Windows-compatible process environment and compatibility DLL surface
- a simple Windows x64 Hello World style program running on x64 host

Exit criteria:

- a small unmodified Windows x64 console program can run on x64 host through
  the Windows user-space personality

## Stage 3: C SDK v0

Goal:

- provide the thinnest possible C interface to the kernel ABI

Deliverables:

- headers for syscalls, handles, status codes, IPC messages, and process APIs
- per-architecture syscall thunks hidden behind common C functions
- build rules to install SDK headers and a small static library into the sysroot

Exit criteria:

- a C program can include the SDK headers and issue raw syscalls without libc

## Stage 4: First User Program Without libc

Goal:

- prove that the kernel ABI is usable before higher-level runtime work starts

Deliverables:

- a statically linked SDK-only sample program
- one observable output path, likely a debug log or console-write syscall
- an automated test that boots the kernel and confirms the program executed

Exit criteria:

- the first Hello World equivalent prints through the SDK alone

## Stage 5: Core User-Space Services

Goal:

- introduce the minimum services that libc will need to feel hosted

Deliverables:

- a name-service process for discovery
- a console or logging service for stdout and stderr
- a first filesystem or pseudo-filesystem surface for file descriptors or
  handles

Exit criteria:

- libc can rely on stable service endpoints instead of special-case kernel
  hooks

## Stage 6: libc v0

Goal:

- make basic hosted C programs possible without chasing full POSIX yet

Deliverables:

- crt objects, errno handling, and startup or teardown glue
- freestanding string and memory routines plus malloc or a minimal allocator
- stdio backed by the console service, including puts and a minimal printf path
- process termination and basic file-descriptor style APIs

Exit criteria:

- a normal C Hello World using libc builds and runs under QEMU

## Stage 7: Sysroot and Toolchain Packaging

Goal:

- make the target environment consumable by external build tools

Deliverables:

- a sysroot layout containing headers, libraries, crt objects, and linker data
- stable target triples for x64 and arm64
- compiler-rt built for those targets and wired into the sysroot

Exit criteria:

- Clang can compile and link C programs against the ringos sysroot without
  ad-hoc local patches

## Stage 8: Clang Bring-Up

Goal:

- teach the compiler to target ringos as a normal hosted platform

Deliverables:

- target definitions for `x86_64-unknown-ringos-msvc` and
  `aarch64-unknown-ringos-msvc`
- driver behavior and compiler-relative default search paths for the sysroot
- automated builds of the Clang toolchain against the ringos target
- at least one end-to-end C build that uses the generated compiler and sysroot

Exit criteria:

- the project can self-host simple user-space C programs with the ringos-aware
  Clang build without relying on bootstrap config files for the common hosted C
  path

## Stage 9: Hosted C++ Runtime Stack

Goal:

- move from hosted C to practical hosted C++

Deliverables:

- libunwind for stack unwinding
- libc++abi for C++ ABI support
- libc++ for the standard C++ library
- validation of exceptions, new or delete, RTTI, and basic containers or
  strings

Exit criteria:

- a simple C++ program using the standard library builds and runs on ringos

## Stage 10: HelloWorld Milestone

Goal:

- package the whole path into a repeatable demonstration target

Deliverables:

- one C Hello World program using libc
- one C++ Hello World or small standard-library sample using the hosted C++
  stack
- CI or scripted coverage that boots both architectures and validates the user
  program output

Exit criteria:

- both architectures can boot the kernel, start a user program, and run a
  hosted C and C++ sample through the same toolchain flow

## Stage 11: Windows Compatibility Through Translation

Goal:

- combine the earlier Windows personality proof with the cross-architecture
  execution path after the ringos-native SDK, libc, and toolchain milestones
  are already in place

Deliverables:

- the same Windows x64 proof program from Stage 2B running on arm64 host
- interoperability between the x64 execution backend and the Windows user-space
  personality components
- a repeatable validation path for the Windows-compatible sample on both x64
  and arm64 hosts

Exit criteria:

- one Windows x64 Hello World style program can run on x64 natively and on
  arm64 through translation

## Suggested Constraints for the First Cut

- prefer static linking first; shared libraries can wait
- prefer one process per address space and one thread per process initially
- keep the first libc scope small and practical rather than claiming broad
  POSIX compatibility early
- avoid baking libc assumptions into the syscall ABI
- keep architecture-specific logic in crt or syscall thunk code, not in libc

## Recommended Next Step From The Current State

The repository has already reached the original Stage 2 proof point for a
minimal x64 user image. The broader interoperability design is documented in
[docs/system_interoperability.md](docs/system_interoperability.md).

The next step should still be Stage 2A, not Stage 2B.

The recommended order is:

1. run the same minimal x64 proof image on arm64 through the emulation path
2. run a small Windows x64 Hello World style executable on x64 host
3. continue with the existing Stage 3 through Stage 10 ringos-native bring-up
4. run that same Windows x64 executable on arm64 through translation as the
  later Stage 11 interoperability milestone

This ordering keeps the two major risks separate:

- cross-architecture execution and emulation
- Windows-compatible user-space personality

It also keeps the deeper Windows interoperability expansion from delaying the
core ringos-native SDK, libc, and toolchain path.

If the project tries to solve both at once, failures will be much harder to
attribute and debug.
