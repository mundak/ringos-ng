# Architecture Overview

ringos-ng is a microkernel operating system targeting x64 and arm64. This
document describes the high-level design, the role of the kernel, how drivers
and services are structured, and how components communicate.

---

## Design Philosophy

The system follows a **microkernel** architecture. The kernel itself is kept as
small as possible and provides only the mechanisms that *must* run in privileged
mode. Everything else — drivers, filesystems, network stacks, and higher-level
services — runs as isolated user-space processes.

Key goals:

- **Isolation.** A fault in one driver cannot corrupt another driver or the
  kernel.
- **Minimal privilege.** Each component runs with exactly the permissions it
  needs.
- **Portability.** Architecture-specific code is confined to thin layers in
  `arch/`; the rest of the system is target-agnostic.

---

## Kernel Responsibilities

The microkernel handles a deliberately narrow set of concerns:

| Responsibility              | Description                                                        |
| --------------------------- | ------------------------------------------------------------------ |
| Address-space management    | Create, destroy, and switch page tables for processes.             |
| Process / thread management | Create, schedule, suspend, and terminate execution contexts.       |
| IPC primitives              | Deliver messages between processes via the RPC mechanism.          |
| Interrupt routing           | Receive hardware interrupts and forward them to user-space drivers.|
| Capability / access control | Track which processes may access which kernel objects.             |

The kernel does **not** include:

- Device drivers (disk, network, USB, display, …)
- Filesystem logic
- Protocol stacks
- Application-level policy

---

## Drivers As Isolated Processes

Each driver runs as a **separate user-space process** with its own virtual
address space. The kernel assigns each driver only the physical memory regions
and I/O resources it requires (e.g. MMIO ranges, I/O ports, interrupt lines).

```
┌──────────────────────────────────────────────────────────┐
│                      User Space                          │
│                                                          │
│  ┌────────────┐  ┌────────────┐  ┌────────────────────┐ │
│  │ Disk Driver │  │ Net Driver │  │ Filesystem Service │ │
│  │ (process)   │  │ (process)  │  │ (process)          │ │
│  └──────┬─────┘  └──────┬─────┘  └─────────┬──────────┘ │
│         │               │                   │            │
│ ────────┼───────────────┼───────────────────┼────────── │
│         │          RPC / IPC                │            │
│ ────────┼───────────────┼───────────────────┼────────── │
│         │               │                   │            │
│  ┌──────┴───────────────┴───────────────────┴──────────┐ │
│  │                   Microkernel                       │ │
│  │  scheduling · address spaces · IPC · IRQ routing    │ │
│  └─────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────┘
```

### Memory Isolation

Every driver process has a private page table managed by the kernel. Shared
memory regions can be explicitly granted between cooperating processes through
kernel-mediated capabilities, but no process can access another's memory by
default.

### Interrupt Delivery

When a hardware interrupt fires, the kernel acknowledges it and sends a
notification message to the registered user-space driver. The driver handles
the interrupt in its own context and replies when finished.

---

## Inter-Process Communication: Syscall-Based RPC

All communication between user-space components uses a **synchronous RPC
mechanism** built on top of the `syscall` instruction.

### Call Flow

1. The **client** process populates a message buffer with the target service
   identifier, the method identifier, and the serialized arguments.
2. The client executes a `syscall` instruction to trap into the kernel.
3. The kernel validates the request, suspends the client, and dispatches the
   message to the **server** process.
4. The server wakes, processes the request, writes a reply into its reply
   buffer, and executes a `syscall` to return the result.
5. The kernel copies the reply back to the client and resumes it.

```
 Client Process              Kernel                 Server Process
 ──────────────              ──────                 ──────────────
  prepare msg
  syscall ──────────────►  validate & route
                            suspend client
                            wake server  ──────────► receive msg
                                                     process request
                                                     prepare reply
                            copy reply   ◄────────── syscall (reply)
  resume  ◄──────────────  resume client
  read reply
```

### Syscall Interface

The RPC mechanism is exposed through a small set of syscalls:

| Syscall      | Purpose                                                      |
| ------------ | ------------------------------------------------------------ |
| `rpc_call`   | Send a request and block until the reply arrives.            |
| `rpc_recv`   | Block until an incoming request arrives (server side).       |
| `rpc_reply`  | Send a reply to a pending request and unblock the caller.    |
| `rpc_signal` | Send a one-way asynchronous notification (no reply expected).|

All syscalls enter the kernel through the architecture's native fast-path
instruction:

| Architecture | Instruction |
| ------------ | ----------- |
| x64          | `syscall`   |
| arm64        | `svc`       |

### Message Format

Messages are fixed-size structures to avoid dynamic allocation inside the
kernel. A message contains:

- **Target** — the service or endpoint identifier.
- **Method** — the operation the client is requesting.
- **Payload** — a small inline data region for arguments / return values.
- **Capabilities** — optional kernel object references (memory grants, endpoint
  handles) transferred between processes.

For bulk data that exceeds the inline payload, processes negotiate a shared
memory region via a capability transfer and reference it by handle in subsequent
messages.

---

## Service Discovery

A dedicated **name server** process (the first user-space process started by the
kernel) maintains a registry of available services. When a driver or service
starts, it registers its endpoint with the name server. Clients look up services
by name and receive an endpoint capability they can use for subsequent RPC calls.

---

## Architecture-Specific Boundaries

Architecture-dependent code lives exclusively under `arch/<target>/`. Each
architecture directory provides:

- **Boot entry** — early startup code that prepares CPU state and jumps to the
  shared kernel entry point.
- **Syscall entry / exit** — the trap handler that dispatches `syscall` / `svc`
  into the kernel's architecture-independent IPC path.
- **Context switch** — saving and restoring register state on process switches.
- **Page table manipulation** — architecture-specific page table formats and TLB
  management.
- **Interrupt controller** — APIC (x64) or GIC (arm64) initialization and
  routing.

The shared kernel code in `kernel/` is written to be fully architecture-neutral.

---

## Future Considerations

The following topics are intentionally out of scope for milestone one but are
part of the long-term architecture:

- SMP and per-core scheduling.
- Asynchronous IPC and batched message delivery.
- Capability-based security model refinement.
- User-space pager and demand paging.
- Device-tree / ACPI enumeration forwarded to user-space.
- Filesystem, network, and storage service implementations.
