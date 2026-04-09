# Codebase Audit: Documentation vs Implementation

This audit compares the current ringos-ng implementation against the requirements
in `contributing.md`, `user-space-abi.md`, `system_interoperability.md`, and
`ci-and-testing.md`. It identifies shortcuts that should be addressed before
implementing new features.

---

## Critical Architectural Violations

### 1. Windows compatibility code lives in kernel space

`system_interoperability.md` explicitly states: *"The Windows compatibility layer
should live in user space, not in kernel space. That is a design rule, not an
optimization."*

Current violations:

- `kernel/user_runtime.cpp` includes `x64_windows_compat.h` and implements
  `SYSCALL_WINDOWS_GET_STD_HANDLE`, `SYSCALL_WINDOWS_WRITE_FILE`, and
  `SYSCALL_WINDOWS_EXIT_PROCESS` directly in the kernel syscall dispatcher.
- `kernel/CMakeLists.txt` links `win32/include` headers into the kernel build.
- The PE loader library (`ringos_x64_win32_emulation`) is linked into the kernel
  executable via `arch/x64/CMakeLists.txt`.
- Windows handle constants (`WINDOWS_HANDLE_STDIN`, `WINDOWS_HANDLE_STDOUT`,
  `WINDOWS_HANDLE_STDERR`) are hardcoded in the kernel.

This is the single largest architectural shortcut. The fix requires a user-space
Windows personality service that translates Windows API calls into ringos RPC
operations, with the kernel exposing only ringos-native mechanisms.

### 2. No per-process handle tables

`user-space-abi.md` mandates: *"A handle is a process-local opaque 64-bit
token... The kernel maps each handle to a live object reference in the caller's
handle table."*

Currently all handles live in global kernel object pools with a shared counter.
There is no per-process handle table and no ownership validation. Any process can
use any handle value if it guesses correctly. The `process` class has no handle
table member at all. This works for single-process Stage 0 but violates the
documented isolation model and must be fixed before multi-process support.

### 3. Process metadata not captured

`system_interoperability.md` says process creation should capture: executable
image format, guest architecture, process personality, execution backend, initial
address-space layout, and initial thread state.

The actual `process` class only stores an `address_space`, an assist channel
pointer, and a device memory pointer. There is no way to distinguish a
ringos-native process from a Windows-compatible one, or to select an execution
backend per-process.

### 4. No host/guest context distinction in threads

`system_interoperability.md` requires thread state to *"distinguish between host
execution context used by the kernel scheduler and guest CPU context used by the
active execution backend."*

The `thread` class has a single `thread_context` containing only an instruction
pointer, stack pointer, flags, and one argument. The ARM64 emulator bridge
reconstructs x64 guest syscall context ad-hoc, duplicating logic. There is no
formal guest CPU context slot on the thread object.

---

## Moderate Gaps

### 5. Syscall arguments 4 and 5 silently dropped

The ABI defines 6 arguments per syscall (`rdi/rsi/rdx/r10/r8/r9` on x64,
`x0-x5` on ARM64). The `user_syscall_context` struct in
`kernel/include/user_runtime_types.h` only carries 4 arguments. Both trap entry
paths save the full register frame but never copy arguments 4 and 5 into the
dispatch context. Current syscalls use at most 3 arguments so this is silent
today, but `ringos_syscall5` or `ringos_syscall6` calls would lose data.

### 6. Device memory objects out of spec

`device_memory_object` and `SYSCALL_DEVICE_MEMORY_MAP` (syscall number 6) are
implemented in the kernel but are not part of the Stage 0 kernel object set
defined in `user-space-abi.md`. The feature was added early to support console
device access.

### 7. Missing SDK wrappers

`RINGOS_SYSCALL_DEVICE_MEMORY_MAP` is defined in the syscall header but has no
user-space convenience wrapper function in the SDK. Shared memory map operations
also lack an SDK wrapper.

### 8. ARM64 CRT not ported

The CRT startup code in `user/crt/src/crt0.c` uses Windows PE-specific
`#pragma section` and `__cdecl` conventions for `.CRT$X*` initializer
processing. There is no ARM64 equivalent, even though the ARM64 target also
builds PE images.

### 9. Heap allocator stubbed

`malloc`, `free`, `calloc`, and `realloc` all return `nullptr` and set
`ENOMEM`. This is intentional for Stage 0 (static linking only) but blocks any
user program that needs dynamic allocation.

---

## What Is Done Well

- **Coding style compliance.** The codebase is highly conformant to
  `contributing.md`: proper `snake_case` naming, fixed-width types, anonymous
  namespaces instead of `static` free functions, function definitions in `.cpp`
  files, template definitions in `.inl` files, correct enum style with explicit
  underlying types and prefixed `UPPER_SNAKE_CASE` values.

- **User pointer validation.** Every syscall validates user pointers through
  `try_translate_user_address()` with integer overflow checks before any
  dereference. The three-tier validation (syscall argument check, address range
  validation, per-operation copy) creates defense-in-depth.

- **Status codes.** All 13 `status_t` values match the ABI document exactly
  (`STATUS_OK` through `STATUS_NOT_FOUND`).

- **Channel IPC.** Synchronous request-reply over paired channel endpoints works
  correctly. `create_channel_pair()` produces two connected endpoints, and the
  `dispatch_rpc_call` path blocks the client until the server replies.

- **Emulator layer separation.** Guest CPU state is properly isolated in
  `x64_emulator_state` (full GP registers, SIMD registers, flags, instruction
  pointer), separate from host thread context. The execution layer correctly owns
  guest instruction decoding and memory access through virtualized callbacks.

- **SDK syscall thunks.** Register conventions are correct on both architectures.
  The x64 thunk avoids `rcx` (clobbered by `syscall`), and the ARM64 thunk
  follows AAPCS64.

- **Test preset alignment.** All 11 test presets documented in
  `ci-and-testing.md` exist in `CMakePresets.json` with no undocumented extras.

- **Security posture.** No dynamic allocation in the kernel (static pools with
  placement new only), no buffer overflow paths in copy operations, proper bounds
  checking on console line buffers and RPC name buffers.

---

## Recommended Priority

| Priority | Item | Effort |
|----------|------|--------|
| P0 | Move Windows compat out of kernel into user-space personality service | Large |
| P0 | Add per-process handle tables with ownership validation | Medium |
| P1 | Add process metadata (format, arch, personality, backend) | Medium |
| P1 | Add host/guest context distinction to thread objects | Medium |
| P1 | Wire syscall arguments 4-5 through to dispatch | Small |
| P2 | Port CRT startup to ARM64 | Small |
| P2 | Add SDK wrappers for device memory map and shared memory | Small |
| P2 | Expand emulator unit test coverage (~28% of implemented instructions) | Medium |

The P0 items represent fundamental architectural debt where the kernel is doing
things the documentation explicitly says it should not. Fixing these before
adding new features prevents compounding the violations.
