# Cleanup Phases And Progress

This document turns the cleanup backlog into a phased execution plan and a
living progress tracker.

Decisions:
- Per-process handle table conversion is out of scope for this plan.
- The intended direction is shareable handles, not process-local handles.
- Follow-up design work should define ownership, rights, duplication, and
  transfer semantics for that model before multi-process features depend on it.

While doing cleanup, don't touch cleanup-opus.md doc.

## Status Legend

- `[ ]` Planned
- `[~]` In progress
- `[x]` Done
- `[-]` Deferred or intentionally out of scope for this plan

## Current Phase Summary

| Phase | Status | Goal |
| --- | --- | --- |
| 0 | `[x]` | Align the written contract with the current repo and handle direction |
| 1 | `[x]` | Introduce a canonical process and syscall model |
| 2 | `[ ]` | Move Windows compatibility policy out of the kernel |
| 3 | `[ ]` | Replace special-case runtime plumbing with generic service and object plumbing |
| 4 | `[ ]` | Generalize the memory model and separate loading from execution layout |
| 5 | `[ ]` | Separate guest faults from backend failures in the emulator path |
| 6 | `[ ]` | Simplify sample integration and bring CI and docs back into alignment |

## Explicit Non-Goals

- `[-]` Converting global handles into per-process handle tables.

## Cross-Cutting Decisions To Record Early

- Handle direction is shareable handles, not process-local handle tables.
  Ownership, rights, duplication, and transfer semantics remain follow-up
  design work.
- Keep lane names identical across documentation, scripts, workflows, and build
  targets.

## Phase 0: Contract Alignment

Status: `[x]`

Goal: make the written design match the repo and capture the shareable-handle
decision before larger refactors start.

Progress:

- `[x]` Resolved the CI source-of-truth question in favor of direct CMake and
  CTest commands plus repo-owned scripts and workflow entry points.
  `CMakePresets.json` is not part of the repo contract.
- `[x]` Updated [ci-and-testing.md](./ci-and-testing.md) to match the current
  workflow and script surface and added a source-of-truth table.
- `[x]` Marked stale preset assumptions in older cleanup notes.
- `[x]` Updated [user-space-abi.md](./user-space-abi.md) to match the chosen
  shareable-handle direction.
- `[x]` Removed the remaining handle-direction mismatch from older cleanup
  notes.

Work items:

- Update [cleanup-opus.md](./cleanup-opus.md) so it no longer acts as the
  authoritative source for handle direction and stale CI assumptions.
- Update [user-space-abi.md](./user-space-abi.md) if handles are meant to stay
  shareable rather than process-local.
- Audit [ci-and-testing.md](./ci-and-testing.md) against the current repo
  layout and workflow surface.
- Produce a simple source-of-truth table that maps each active workflow in
  [.github/workflows](../.github/workflows) to its actual script or build entry
  point.
- Mark stale assumptions in older cleanup notes where they no longer match the
  current tree.

Primary files:

- [user-space-abi.md](./user-space-abi.md)
- [ci-and-testing.md](./ci-and-testing.md)

Verification:

- No cleanup document still treats per-process handle tables as the committed
  direction.
- The CI document names only workflows, scripts, and build entry points that
  actually exist in the repo, and it flags stale workflow files separately.

## Phase 1: Canonical Process And Syscall Model

Status: `[x]`

Goal: give the runtime one canonical description of what a process is and one
canonical representation of syscall entry, regardless of native or translated
execution.

Progress:

- `[x]` Extended [../kernel/process.h](../kernel/process.h) and
  [../kernel/user_runtime_types.h](../kernel/user_runtime_types.h) with
  canonical process metadata for guest architecture, personality, and
  execution backend.
- `[x]` Replaced the partial syscall snapshot with one canonical
  `user_syscall_context` carrying trap instruction pointer, stack pointer,
  flags, and six syscall arguments.
- `[x]` Routed native x64, native arm64, and translated x64-on-arm64 through
  the same syscall-context construction contract and moved thread user-context
  synchronization into [../kernel/user_runtime.cpp](../kernel/user_runtime.cpp).
- `[x]` Recorded per-process metadata during initial bootstrap creation and
  fixed dispatch-path truncation so syscall arguments 4 and 5 are preserved.

Work items:

- Extend [../kernel/process.h](../kernel/process.h) with process metadata for
  guest architecture, personality, and execution backend.
- Extend [../kernel/user_runtime_types.h](../kernel/user_runtime_types.h) so
  syscall context carries the full dispatch information needed by the runtime,
  including trap instruction pointer, stack pointer, flags, and all guest
  syscall arguments.
- Add one canonical syscall-context construction path for native x64 in
  [../arch/x64/user_runtime.cpp](../arch/x64/user_runtime.cpp).
- Add one canonical syscall-context construction path for arm64 and the x64-on-
  arm64 bridge in [../arch/arm64/user_runtime.cpp](../arch/arm64/user_runtime.cpp)
  and [../arch/arm64/arm64_initial_user_runtime_platform.cpp](../arch/arm64/arm64_initial_user_runtime_platform.cpp).
- Route [../kernel/user_runtime.cpp](../kernel/user_runtime.cpp) through that
  canonical structure instead of path-specific assumptions.
- Fix the current syscall argument truncation so arguments 4 and 5 are not lost
  in the dispatch path.

Primary files:

- [../kernel/process.h](../kernel/process.h)
- [../kernel/user_runtime_types.h](../kernel/user_runtime_types.h)
- [../kernel/user_runtime.cpp](../kernel/user_runtime.cpp)
- [../arch/x64/user_runtime.cpp](../arch/x64/user_runtime.cpp)
- [../arch/arm64/user_runtime.cpp](../arch/arm64/user_runtime.cpp)
- [../arch/arm64/arm64_initial_user_runtime_platform.cpp](../arch/arm64/arm64_initial_user_runtime_platform.cpp)

Verification:

- Native x64 and translated x64-on-arm64 build the same canonical syscall
  shape for equivalent calls.
- Syscalls with five or six arguments can flow through dispatch without losing
  register state.
- Process creation records enough metadata to route later behavior by guest ISA
  and personality.

## Phase 2: Personality Boundary And Windows Extraction

Status: `[ ]`

Goal: move Windows-specific policy and compatibility behavior out of the kernel
boundary so the kernel stays ringos-native and personality-neutral.

Work items:

- Remove Windows-specific syscall numbers from
  [../kernel/user_space.h](../kernel/user_space.h).
- Remove Windows-specific dispatch cases from
  [../kernel/user_runtime.cpp](../kernel/user_runtime.cpp).
- Stop including Windows compatibility headers directly from kernel runtime
  paths.
- Stop linking `ringos_x64_win32_emulation` into kernel targets from
  [../arch/x64/CMakeLists.txt](../arch/x64/CMakeLists.txt) and
  [../arch/arm64/CMakeLists.txt](../arch/arm64/CMakeLists.txt).
- Introduce a user-space personality boundary for import resolution, process
  environment setup, and compatibility shims.

Primary files:

- [../kernel/user_space.h](../kernel/user_space.h)
- [../kernel/user_runtime.cpp](../kernel/user_runtime.cpp)
- [../arch/x64/CMakeLists.txt](../arch/x64/CMakeLists.txt)
- [../arch/arm64/CMakeLists.txt](../arch/arm64/CMakeLists.txt)
- [../arch/x64/user_runtime.cpp](../arch/x64/user_runtime.cpp)
- [../arch/arm64/arm64_initial_user_runtime_platform.cpp](../arch/arm64/arm64_initial_user_runtime_platform.cpp)

Verification:

- Kernel targets build without Windows emulation linked into them.
- Windows compatibility behavior is either moved behind a user-space layer or
  fails cleanly at a documented boundary instead of being implemented inside
  the kernel dispatcher.

## Phase 3: Generic Service And Object Plumbing

Status: `[ ]`

Goal: remove proof-path special cases and expose reusable service and shared
memory primitives instead of hardwiring behavior into the runtime.

Work items:

- Introduce a generic service registration and lookup mechanism on top of the
  existing RPC substrate in [../kernel/rpc.h](../kernel/rpc.h) and
  [../kernel/user_runtime.h](../kernel/user_runtime.h).
- Treat console support as one registered service implementation rather than a
  special runtime path.
- Expose shared-memory and device-memory concepts through public SDK headers
  instead of leaving them as internal plumbing.
- Replace the `STATUS_NOT_FOUND` stub path for `SYSCALL_DEVICE_MEMORY_MAP` in
  [../kernel/user_runtime.cpp](../kernel/user_runtime.cpp) with a real object-
  backed mapping flow, or remove the syscall until the public model is ready.
- Keep the public ABI focused on generic objects, mappings, and channels rather
  than service-specific shortcuts.

Primary files:

- [../kernel/rpc.h](../kernel/rpc.h)
- [../kernel/user_runtime.h](../kernel/user_runtime.h)
- [../kernel/user_runtime.cpp](../kernel/user_runtime.cpp)
- [../user/sdk/include/ringos/sdk.h](../user/sdk/include/ringos/sdk.h)

Verification:

- Console behavior goes through the same registration and lookup path as any
  other service.
- Shared memory and device memory have a documented public surface instead of
  being runtime-only internals.

## Phase 4: Memory Model And Loader Separation

Status: `[ ]`

Goal: replace the current fixed bootstrap layout with an explicit mapping model
and separate image loading from how guest memory is backed and executed.

Work items:

- Replace the single linear user-to-host translation assumption in
  [../kernel/user_runtime_types.h](../kernel/user_runtime_types.h) and
  [../kernel/user_runtime.cpp](../kernel/user_runtime.cpp) with an explicit
  mapping representation.
- Stop storing device-memory-specific address fields directly inside
  `address_space` once object-backed mappings exist.
- Treat RPC transfer regions and shared-memory windows as mappings owned by
  objects rather than fixed address-space fields.
- Keep image loading logic in the architecture bootstrap layers separate from
  the mapping model used by the runtime.
- Prepare for multiple mappings and relocation-friendly loading without baking
  the current demo image layout deeper into the kernel.

Primary files:

- [../kernel/user_runtime_types.h](../kernel/user_runtime_types.h)
- [../kernel/user_runtime.cpp](../kernel/user_runtime.cpp)
- [../arch/x64/user_runtime.cpp](../arch/x64/user_runtime.cpp)
- [../arch/arm64/arm64_initial_user_runtime_platform.cpp](../arch/arm64/arm64_initial_user_runtime_platform.cpp)

Verification:

- User address translation no longer assumes a single `user_host_base` mapping.
- The runtime can represent more than one user-visible mapping per process.
- Image loading and runtime memory translation can evolve independently.

## Phase 5: Emulator Fault Model Cleanup

Status: `[ ]`

Goal: distinguish guest faults from backend or configuration failures so the
translated execution path can report user faults without panicking the kernel.

Work items:

- Split guest stop reasons from backend failure reasons in
  [../emulator/include/x64_emulator.h](../emulator/include/x64_emulator.h).
- Return structured trap or fault data from the emulator result path.
- Update the arm64 translated runtime path to interpret guest faults as process-
  level events and backend failures as kernel bugs.
- Route that decision through shared runtime logic rather than arch-specific
  panic behavior.

Primary files:

- [../emulator/include/x64_emulator.h](../emulator/include/x64_emulator.h)
- [../arch/arm64/arm64_initial_user_runtime_platform.cpp](../arch/arm64/arm64_initial_user_runtime_platform.cpp)
- [../kernel/user_runtime.cpp](../kernel/user_runtime.cpp)

Verification:

- An invalid guest memory access becomes a process fault or structured runtime
  event, not an unconditional kernel panic.
- Backend misconfiguration and guest faults are distinguishable in logs and
  control flow.

## Phase 6: Sample, Test, CI, And Documentation Alignment

Status: `[ ]`

Goal: reduce bespoke sample integration and make the actual build and CI
contract easy to understand and maintain.

Work items:

- Replace manual sample-lane enumeration in
  [../arch/x64/CMakeLists.txt](../arch/x64/CMakeLists.txt),
  [../arch/arm64/CMakeLists.txt](../arch/arm64/CMakeLists.txt), and
  [../tests/build-tests.sh](../tests/build-tests.sh) with a more generic sample
  test harness.
- Keep the installed toolchain bundle as the only sample-facing toolchain
  interface.
- Decide whether the build contract should be driven by scripts, workflows, or
  restored presets, then align the repo around that choice.
- Align lane names across [ci-and-testing.md](./ci-and-testing.md), workflow
  files under [.github/workflows](../.github/workflows), and sample test
  scripts.
- Restore explicit unit-test coverage for the emulator and Win32 loader in CI,
  or update the documentation to describe the intentionally narrower workflow
  surface.

Primary files:

- [../arch/x64/CMakeLists.txt](../arch/x64/CMakeLists.txt)
- [../arch/arm64/CMakeLists.txt](../arch/arm64/CMakeLists.txt)
- [../tests/build-tests.sh](../tests/build-tests.sh)
- [./ci-and-testing.md](./ci-and-testing.md)
- [../.github/workflows/test-hello-world.yml](../.github/workflows/test-hello-world.yml)
- [../.github/workflows/test-hello-world-cpp.yml](../.github/workflows/test-hello-world-cpp.yml)
- [../.github/workflows/test-console-service-write.yml](../.github/workflows/test-console-service-write.yml)

Verification:

- Adding a new sample or lane does not require duplicating architecture-
  specific build logic in multiple places.
- The documentation describes only the CI surface that actually exists.
- Emulator and Win32 loader coverage are either present in CI or explicitly
  documented as deferred.

## Suggested Execution Order

1. Phase 0
2. Phase 1
3. Phase 2
4. Phase 3
5. Phase 4
6. Phase 5
7. Phase 6

That order keeps the highest-risk kernel-boundary decisions ahead of broader
runtime generalization and leaves CI cleanup until the technical surface has
stabilized.
