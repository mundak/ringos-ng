# User-Space ABI v0

This document freezes the Stage 0 execution model for the first ringos user-space bring-up.
It is intentionally small: it defines the initial syscall boundary, kernel object model,
handle rules, status codes, and first user image formats without pulling libc or POSIX
assumptions into the kernel ABI.

This contract applies to the first statically linked SDK-only user programs. Later stages may
add services, libc, a sysroot, and ringos-specific compiler integration, but those layers should
build on this ABI rather than replace it.

## Scope

- Static linking only.
- One process per address space.
- One thread per process in the first cut.
- Synchronous request-reply IPC over channels.
- Handles are opaque 64-bit shareable object references.
- Architecture-specific trap details stay behind thin per-target SDK thunks.

## Architecture-Neutral ABI Rules

### Syscall Numbering

- Syscall numbers are architecture-neutral 32-bit values.
- Both targets expose the same syscall numbers and the same semantic contract.
- The SDK owns the per-architecture assembly thunks that map C calls onto the trap ABI.
- State that does not fit the register budget must be passed through a pointer into a mapped shared-memory region.

### Scalar And Pointer Conventions

- Integer and pointer arguments cross the syscall boundary in general-purpose registers only.
- Floating-point and SIMD arguments are not part of the Stage 0 syscall ABI.
- Output values are returned in registers when practical; larger outputs must be written through pointers into mapped shared memory.
- The primary return register always carries a `status_t` value.
- On failure, output buffers are unchanged unless a specific syscall says otherwise.
- All user pointers are validated against the caller's address space before mutation.

### Register Usage By Architecture

| Architecture | Trap instruction | Syscall number | Argument registers | Return register | Notes |
| --- | --- | --- | --- | --- | --- |
| x64 | `syscall` | `rax` | `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9` | `rax` | `rcx` and `r11` are clobbered by the instruction. |
| arm64 | `svc #0` | `x8` | `x0`-`x5` | `x0` | Caller-saved register rules follow AAPCS64. |

The x64 register choice deliberately avoids `rcx` because the hardware `syscall` path overwrites
it. This keeps the raw trap ABI simple and keeps the SDK thunks predictable on both targets.

## Execution Model

### Process Model

- A process owns one user address space and one initial thread.
- The initial implementation supports exactly one thread per process.
- There is no `fork`-style duplication model in the first cut.
- Handles are not specified as process-local table entries in Stage 0.
- A process starts at the image entry point of a statically linked user executable.
- The detailed initial stack layout is deferred to Stage 2, but Stage 0 requires the loader to
  enter user mode with a valid, aligned stack and no ambient kernel pointers.

### Thread Model

- A thread is the schedulable execution context inside a process.
- In Stage 0, threads exist as kernel objects because the scheduler and trap return path need
  them, but only one user thread per process is supported.
- Future multi-threading must extend this model without changing handle semantics.

## Kernel Object Types

The first kernel object set is intentionally minimal and matches the next implementation stage.

| Object type | Purpose | Initial notes |
| --- | --- | --- |
| `process` | Owns an address space and initial thread | Created explicitly; no implicit inheritance. |
| `thread` | Represents a schedulable user execution context | Initially one thread per process. |
| `channel` | Carries synchronous request-reply IPC | Created as endpoint pairs. |
| `shared_memory` | Represents explicitly granted shared pages | Used for bulk data instead of oversized copied messages. |

No filesystem objects, sockets, interrupts, signals, or driver-facing device handles are part of
the Stage 0 contract.

## Handle Rules

### Handle Semantics

- A handle is an opaque 64-bit token that refers to a kernel-managed object.
- Handles follow a shareable-object direction rather than a per-process handle-table model.
- The kernel resolves each live handle to an object reference and validates the requested operation.
- A process may use any operation that is valid for an object it already holds a handle to.
- A process does not gain ambient access to another process's address space or to kernel memory.
- Stage 0 does not yet freeze ownership, rights reduction, duplication, or transfer semantics.
- There is no implicit inheritance model in the first cut.

## IPC Model

### Channel Contract

- Channels are the only Stage 0 kernel IPC primitive.
- A channel is created as two connected endpoints.
- IPC is synchronous at the semantic level: clients issue a request and receive exactly one reply.
- An RPC call is function-call-like: it names a target endpoint, an operation number, and a small set of scalar register arguments.
- State that does not fit in registers must be passed indirectly through shared memory that both sides can map.

### RPC Shape

The intended request path is:

1. The client chooses an RPC endpoint handle and an operation number.
2. The client places scalar arguments in registers.
3. If more state is needed, the client passes a pointer into shared memory.
4. The client traps into the kernel with an RPC call operation.
5. The kernel validates the handle and any user pointers, then routes the call to the server endpoint.
6. The server replies exactly once.
7. The kernel resumes the client with scalar return values in registers or larger results through shared memory, plus a final status code.

Stage 0 does not define byte-message payloads, one-way asynchronous sends, broadcast delivery,
inherited mailboxes, or signal objects. Those can be added later if needed, but the first ABI
commits only to blocking request-reply semantics.

## Status Codes

`status_t` is a signed 32-bit integer. `0` means success. All failures are negative values.

| Name | Value | Meaning |
| --- | --- | --- |
| `STATUS_OK` | `0` | Success. |
| `STATUS_INVALID_ARGUMENT` | `-1` | An argument value or combination is invalid. |
| `STATUS_BAD_HANDLE` | `-2` | The supplied handle does not name a live object reference the caller may use. |
| `STATUS_WRONG_TYPE` | `-3` | The handle refers to a different object type than required. |
| `STATUS_BUFFER_TOO_SMALL` | `-4` | A caller-provided output or shared-memory buffer is too small. |
| `STATUS_PEER_CLOSED` | `-5` | The opposite channel endpoint has been closed. |
| `STATUS_WOULD_BLOCK` | `-6` | A non-blocking or immediate operation cannot complete now. |
| `STATUS_TIMED_OUT` | `-7` | A bounded wait expired. |
| `STATUS_NO_MEMORY` | `-8` | Kernel memory or address-space resources are exhausted. |
| `STATUS_FAULT` | `-9` | A user pointer could not be read or written safely. |
| `STATUS_NOT_SUPPORTED` | `-10` | The operation is valid in principle but not implemented. |
| `STATUS_BAD_STATE` | `-11` | The object is in a state that forbids the requested operation. |
| `STATUS_NOT_FOUND` | `-12` | The named object or service does not exist. |

These names are kernel-facing and SDK-facing. A future libc layer may translate them into `errno`
or higher-level language conventions without changing the syscall ABI.

## User Image Format And Target Triples

### First Supported User Image Format

The first supported user executable format is architecture-specific and intentionally minimal.

- x64 user images: `PE32+` (`PE64`), `IMAGE_FILE_MACHINE_AMD64`, statically linked, built without
  system libraries, and loaded at a fixed image base for the Stage 2 proof path.
- arm64 user images: `PE32+` (`PE64`), `IMAGE_FILE_MACHINE_ARM64`, statically linked, built without
  system libraries, and loaded at a fixed image base for the early proof path.
- Shared libraries, PIE, a dynamic loader, and relocation processing are out of scope for the
  first cut.

The existing x64 kernel boot path still converts the kernel image to a Multiboot-compatible 32-bit
ELF container for QEMU boot. That is a kernel boot artifact only and is not part of the user-space
image contract.

### First Target Triples

Stage 0 keeps provisional compiler-facing targets for the earliest user-space bring-up:

- x64 Stage 2 proof path: `x86_64-pc-windows-msvc` to emit a PE64 image without system libraries.
- arm64 proof path: `aarch64-pc-windows-msvc` to emit a PE64 image without system libraries.

These names are implementation choices for the current bootstrap path, not a claim about the final
ringos-specific toolchain surface. Stage 8 adopts `x86_64-unknown-ringos-msvc` and
`aarch64-unknown-ringos-msvc` as the real ringos compiler-facing triples while keeping the same
bootstrap PE or COFF image model and underlying syscall ABI.

## Consequences For Stage 1

Stage 1 implementation work should therefore target:

- trap entry and return paths for `syscall` on x64 and `svc #0` on arm64
- user address spaces with one initial thread per process
- 64-bit shareable handles that refer to live kernel objects
- channel endpoints that support synchronous function-call-style RPC
- explicit shared-memory objects for bulk transfer and shared buffers

Any Stage 1 design that changes the shareable-handle direction, reintroduces committed
process-local handle tables, requires implicit handle inheritance, asynchronous message-only IPC,
or a different first user image format should be treated as a deliberate ABI change and updated
here before code is written.
