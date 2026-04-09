# Cleanup Notes Before New Feature Work

This document records the main shortcuts and architectural debt found during a
review of the current codebase against the requirements in:

- [README.md](../README.md)
- [docs/ci-and-testing.md](./ci-and-testing.md)
- [docs/system_interoperability.md](./system_interoperability.md)
- [docs/user-space-abi.md](./user-space-abi.md)

The goal is not to expand scope immediately. The goal is to identify the places
where the current proof-path implementation is likely to force redesign if new
features are added on top of it unchanged.

## Priority Order

The most important cleanup areas are:

1. Replace the global handle model with real per-process handle tables.
2. Introduce a canonical process and syscall model that includes guest
   architecture, personality, and execution backend.
3. Move Windows compatibility and service-specific policy out of the kernel.
4. Replace console-specific bootstrap wiring with generic object and service
   plumbing.
5. Generalize the memory and image execution model so it is not locked to the
   current embedded demo layout.
6. Bring CI, presets, and documentation back into alignment.

## 1. Handle Model Does Not Match The ABI

The user-space ABI says handles are opaque 64-bit process-local object
references and that Stage 1 should use per-process handle tables.

Current implementation:

- [docs/user-space-abi.md](./user-space-abi.md) defines handles as
  process-local.
- [kernel/include/kernel_object_pool.inl](../kernel/include/kernel_object_pool.inl)
  allocates handles from one shared counter.
- [kernel/user_runtime.cpp](../kernel/user_runtime.cpp) resolves handles by
  scanning global object pools.

Why this should be fixed before new features:

- It weakens process isolation.
- It prevents a clean ownership and rights model.
- It will make multi-process features and handle transfer semantics much harder
  to add later.

Recommended cleanup:

- Give each process its own handle table.
- Make object lookup go through the caller process.
- Separate kernel object identity from process-visible handle values.
- Reserve space for rights, duplication, and eventual transfer rules.

## 2. Process And Syscall Context Are Still Too Narrow

The interoperability document calls for a process model that includes guest
architecture, process personality, and execution backend, plus a canonical
request frame for service dispatch.

Current implementation:

- [kernel/include/process.h](../kernel/include/process.h) stores only address
  space data and console-assist pointers.
- [kernel/include/user_runtime_types.h](../kernel/include/user_runtime_types.h)
  defines a syscall context with only syscall number, four arguments, and stack
  pointer.
- [arch/x64/user_runtime.cpp](../arch/x64/user_runtime.cpp),
  [arch/arm64/user_runtime.cpp](../arch/arm64/user_runtime.cpp), and the arm64
  x64 emulator bridge each reconstruct syscall context in slightly different
  ways.

Why this should be fixed before new features:

- It bakes native and translated execution assumptions into trap handlers.
- It leaves no clean place to route behavior by guest ISA or personality.
- It will force duplicated logic as soon as there is more than one translated
  or compatibility path.

Recommended cleanup:

- Extend process metadata with guest architecture, personality, and execution
  backend.
- Extend syscall context with the data called out in
  [docs/system_interoperability.md](./system_interoperability.md): guest
  architecture, service number, trap IP, stack pointer, and relevant flags.
- Route all syscall entry paths through one canonical dispatch structure.

## 3. Windows Compatibility Still Lives Inside The Kernel Boundary

The long-term design says the Windows compatibility layer should live in user
space rather than in the kernel.

Current implementation:

- [kernel/include/user_space.h](../kernel/include/user_space.h) defines Windows
  syscall numbers.
- [kernel/user_runtime.cpp](../kernel/user_runtime.cpp) implements Windows
  compatibility cases directly in the kernel syscall switch.
- [win32/CMakeLists.txt](../win32/CMakeLists.txt) builds the Win32 emulation
  code that is then linked into kernel targets from [arch/x64/CMakeLists.txt](../arch/x64/CMakeLists.txt)
  and [arch/arm64/CMakeLists.txt](../arch/arm64/CMakeLists.txt).
- Initial Windows import resolution is wired into the bootstrap runtime in
  [arch/x64/user_runtime.cpp](../arch/x64/user_runtime.cpp) and
  [arch/arm64/user_runtime.cpp](../arch/arm64/user_runtime.cpp).

Why this should be fixed before new features:

- It mixes mechanism with Windows-specific policy.
- It makes the kernel responsible for growing compatibility semantics.
- It prevents the Windows personality from evolving independently of the
  privileged kernel.

Recommended cleanup:

- Keep the kernel-side interface ringos-native and personality-neutral.
- Move Windows API behavior behind a user-space personality layer.
- Treat import resolution, process environment setup, and compatibility shims
  as user-mode responsibilities.

## 4. Console Bootstrap Special Cases Are Standing In For A General Object Model

The ABI and architecture docs describe channels and shared memory as generic
building blocks. The current implementation is still shaped around the console
driver proof path.

Current implementation:

- [kernel/include/process.h](../kernel/include/process.h) stores an assist
  channel and assist device-memory object directly on each process.
- [kernel/user_runtime.cpp](../kernel/user_runtime.cpp) hardcodes the
  `console.default` endpoint and special-cases direct console RPC dispatch.
- [user/sdk/include/ringos/sdk.h](../user/sdk/include/ringos/sdk.h) exposes a
  console-oriented surface, while shared memory is still internal runtime
  plumbing.

Why this should be fixed before new features:

- New services will otherwise be added as more kernel exceptions.
- Service discovery is not generic.
- Device memory and shared transfer regions are not yet modeled as a reusable
  service mechanism.

Recommended cleanup:

- Remove service-specific pointers from `process`.
- Introduce generic registration and lookup for named services.
- Expose shared-memory and mapping concepts as real public ABI primitives.
- Keep console support as one service implementation, not as a special kernel
  path.

## 5. Memory Layout Is Hardwired To The Current Demo Path

The current address-space structure assumes a simple linear user-to-host mapping
plus one RPC transfer region and one device-memory region.

Current implementation:

- [kernel/include/user_runtime_types.h](../kernel/include/user_runtime_types.h)
  stores `user_host_base`, one RPC transfer window, and one device-memory
  window directly in the address-space descriptor.
- [kernel/user_runtime.cpp](../kernel/user_runtime.cpp) translates user
  addresses with a simple base-plus-offset calculation.
- [arch/x64/user_runtime.cpp](../arch/x64/user_runtime.cpp) and
  [arch/arm64/user_runtime.cpp](../arch/arm64/user_runtime.cpp) populate fixed
  bootstrap regions for the current runtime model.

Why this should be fixed before new features:

- It will not scale cleanly to richer user address spaces.
- It assumes guest-visible and host-visible memory can stay trivially aligned.
- It makes later work such as relocation, multiple mappings, or more realistic
  image loading much harder.

Recommended cleanup:

- Replace the single linear translation assumption with an explicit mapping
  model.
- Treat RPC/shared-memory windows as mappings owned by objects, not as fixed
  address-space fields.
- Keep image loading separate from how guest memory is backed and executed.

## 6. Emulator Faults And Backend Failures Are Not Separated Cleanly

The interoperability document distinguishes guest faults from host/backend
faults. The current translated path does not make that distinction cleanly.

Current implementation:

- [emulator/include/x64_emulator.h](../emulator/include/x64_emulator.h)
  combines guest outcomes and backend/configuration failures in one completion
  enum.
- [arch/arm64/user_runtime.cpp](../arch/arm64/user_runtime.cpp) panics on all
  non-success emulator completions.

Why this should be fixed before new features:

- Guest faults should become process-level events, not always kernel panics.
- Debugging translated execution stays ambiguous.
- Exception delivery and recovery will be difficult to add later.

Recommended cleanup:

- Separate guest stop reasons from backend failure reasons.
- Return structured trap and fault data from the execution backend.
- Let shared runtime code decide whether the event is a user-process fault or a
  kernel bug.

## 7. Sample And Test Integration Is Still Too Bespoke

The repository correctly pushes samples toward the installed toolchain bundle,
but the temporary embedded-app path is still very sample-specific.

Current implementation:

- Standalone samples such as
  [user/samples/hello_world_cpp/CMakeLists.txt](../user/samples/hello_world_cpp/CMakeLists.txt)
  correctly require the installed toolchain bundle.
- The root build graph still includes repo-local toolchain helper modules in
  [CMakeLists.txt](../CMakeLists.txt).
- Kernel targets for sample lanes are manually enumerated in
  [arch/x64/CMakeLists.txt](../arch/x64/CMakeLists.txt),
  [arch/arm64/CMakeLists.txt](../arch/arm64/CMakeLists.txt), and
  [tests/build-tests.sh](../tests/build-tests.sh).

Why this should be fixed before new features:

- Every new sample or personality lane currently expands bespoke build logic.
- The current embedded-app path is useful as a workaround, but it is not a good
  long-term interface.

Recommended cleanup:

- Keep the installed bundle as the only sample-facing toolchain interface.
- Reduce sample-specific kernel target proliferation.
- Move toward a more generic embedded-app test harness or a real loader path as
  soon as the filesystem and process launch model allow it.

## 8. CI Contract And Repository Reality Have Drifted

The CI documentation says the repository should expose twelve separately tracked
workflows and that most workflows should run one scenario-specific CTest preset.

Current implementation:

- [docs/ci-and-testing.md](./ci-and-testing.md) still describes twelve distinct
  workflows.
- [.github/workflows](../.github/workflows) currently contains three matrixed
  sample workflows plus the toolchain release workflow.
- [CMakePresets.json](../CMakePresets.json) still defines the emulator and
  Win32 loader unit-test presets, but there are no matching GitHub Actions
  workflows.
- Naming between docs and presets has drifted for the x64-on-arm64 hello-world
  lane.

Why this should be fixed before new features:

- The documented contract is no longer a reliable source of truth.
- CI visibility for unit-test coverage is weaker than the docs imply.
- Future changes are harder to review when names and execution paths drift.

Recommended cleanup:

- Decide whether the source of truth is the preset surface or the workflow
  surface, then align the other one to match.
- Restore explicit unit-test workflow coverage or update the documentation to
  describe the real matrixed setup.
- Keep lane names identical across docs, presets, shell scripts, and workflows.

## Suggested Execution Order

If this cleanup work is split into phases, a practical order is:

1. Handle tables and process metadata.
2. Canonical syscall and execution-backend boundary.
3. Removal of kernel-resident Windows and console policy.
4. Generic object, memory, and service plumbing.
5. Memory-model cleanup and loader/runtime separation.
6. CI and documentation alignment.

That order addresses the highest-risk architectural debt first and reduces the
chance of implementing new features on top of the wrong kernel boundary.
