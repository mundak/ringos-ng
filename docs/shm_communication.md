# Shared-Memory Communication Design

This document describes a high-throughput interprocess communication transport
that is separate from the existing RPC system.

The intent is to provide a fast path for steady-state communication that avoids
syscalls and interrupts after setup. The kernel remains responsible for
creating, mapping, and tearing down the shared transport objects, but the hot
path runs entirely in user space.

## Goals

- Keep the transport separate from the existing synchronous RPC mechanism.
- Make use of the transport explicit and opt-in per process.
- Support many threads across many processes sending requests concurrently.
- Avoid syscalls and interrupts on the data path.
- Scale under contention without forcing all traffic through one shared queue.
- Preserve a small kernel boundary and clear ownership rules.

## Non-Goals

- Replacing the current RPC system.
- Making transport setup or teardown fully syscall-free.
- Treating peer-provided raw pointers as valid cross-process references.
- Defining network-style reliability, routing, or broadcast semantics.

## Relationship To Existing RPC

This transport should not be modeled as an extension of the current RPC ABI.

The existing RPC path remains appropriate for:

- small control operations
- service bootstrap
- setup and teardown of transport sessions
- slow-path fallback behavior

The shared-memory transport exists for queue-based, high-throughput,
multi-threaded communication where the overhead of kernel-mediated per-message
 dispatch is too high.

## High-Level Model

The transport has two kernel object types and one in-region structure:

1. `shm_endpoint`: a named listener published by a server process (kernel object).
2. `shm_session`: a connection between one client process and one server endpoint (kernel object).
3. Queues: request and completion queues inside the session's mapped region (user-space structures, not kernel objects).

A server explicitly registers an endpoint. A client explicitly opens that
endpoint. The kernel then creates a session object, maps the transport region
into both processes, and returns the session handle plus the user-visible base
address for the mapping.

After that handshake, requests and replies flow entirely through shared memory.

## Opt-In Semantics

Processes do not participate in this transport implicitly.

- A server opts in by creating and publishing a named `shm_endpoint`.
- A client opts in by opening that endpoint.
- A session exists only after both sides complete the kernel-mediated setup.
- A process that never opens the endpoint never pays the cost of the transport.

This keeps the existing RPC model intact while allowing selected services to
expose a faster transport where it matters.

## Session Layout

Each `shm_session` owns one shared-memory region with a fixed internal layout:

- connection control page
- queue directory
- request queue headers
- completion queue headers
- descriptor arrays
- payload arena

The control page should contain at least:

- magic value
- protocol version
- session generation
- peer state
- queue counts and offsets
- payload arena size and offsets

The session generation must change whenever a session is recreated so stale
mappings can be detected reliably.

## Queue Topology

The default design should not use one giant global MPMC queue.

That approach is simple, but it scales poorly because all producers contend on
the same enqueue state and all consumers contend on the same dequeue state.

Instead, the session should be sharded into multiple queues:

- request queues for client-to-server traffic
- completion queues for server-to-client traffic

The preferred fast path is:

- one request queue per producer thread
- one completion queue per producer thread
- one or more server pollers draining groups of request queues

There is one session per client-server connection, with multiple leased queues
inside each session. This keeps the hot path close to SPSC behavior while still
allowing many active threads across many processes.

### Queue Leasing

Each session should support queue leasing.

- A producer thread acquires a queue lease from the queue directory.
- Once leased, that queue has exactly one producer.
- A completion queue is associated with the same producer identity.
- The server routes replies to the completion queue named in the request descriptor.

If there are more producer threads than available queues, multiple threads may
share a queue as a fallback. That fallback may use MPSC semantics, but it is
not the primary performance path.

## Descriptor Format

Queues should carry fixed-size descriptors rather than variable-length inline
messages.

Each descriptor should contain at least:

- `request_id`
- `opcode`
- `flags`
- `status`
- `completion_queue_id`
- `payload_kind`
- `payload_offset`
- `payload_length`
- `user_cookie`

For larger payloads, descriptors reference slices of the shared payload arena.
For very small requests, a compact inline payload may be added later if
measurement shows it matters.

The transport must never send raw process-local pointers between peers.

## Ordering And Completion Rules

Ordering should be defined per queue, not globally across a session.

- Requests submitted on the same queue are observed in submission order.
- Requests on different queues may be processed in any order.
- Replies are matched by `request_id`, not by descriptor slot.
- Multiple requests may be in flight on the same queue.

## Backpressure

Backpressure must be explicit because the hot path has no blocking kernel wait.

Recommended behavior:

- enqueue on full queue returns `STATUS_WOULD_BLOCK`
- dequeue on empty queue returns `STATUS_WOULD_BLOCK`
- queue saturation is visible to user space

The transport should not hide contention behind unbounded busy waits.

## Wait Strategy

The base design assumes polling only with short active spins using architecture
pause or yield hints.

## Lifecycle

The transport is syscall-free only after setup.

The expected lifecycle is:

1. Server creates a named `shm_endpoint` and publishes capabilities.
2. Client opens that endpoint and requests queue and arena sizing.
3. Kernel creates a `shm_session`, maps the region into both processes, and returns handles plus user addresses.
4. Both peers initialize the control page and enter the `LIVE` state.
5. Requests and replies move entirely through shared memory.
6. On close or process death, the kernel tears down the session and invalidates the mapping.

The control page should expose peer state transitions such as:

- `INIT`
- `LIVE`
- `DRAINING`
- `DEAD`

## Failure Detection

The kernel detects peer failure and cleans up:

- revoke mappings on process teardown
- mark the session dead during object cleanup

A live peer can detect a dead session by checking the peer state word and
session generation in the control page.

## Security And Validation

Shared memory removes copying overhead but does not remove trust boundaries.

Rules:

- treat every peer-written descriptor as untrusted input
- validate every offset and length against the mapped region
- never dereference a peer-provided raw pointer
- keep object ownership and mapping lifetime under kernel control

The protocol must be zero-copy where possible, not zero-validation.

## Kernel Object Direction

The kernel-facing object family for this transport should remain separate from
the current channel and RPC objects.

Kernel object types:

- `shm_endpoint` — managed via `kernel_object_pool`, handle-based access.
- `shm_session` — managed via `kernel_object_pool`, handle-based access.

Queues are in-region user-space structures, not kernel objects.

The relationship to the existing `shared_memory_object` should be clarified
during implementation: `shm_session` may wrap or replace it for this transport.

The kernel should own:

- endpoint publication and lookup
- session creation and teardown
- mapping creation and revocation
- handle-based access control
- peer-death cleanup

The kernel should not participate in per-message routing after the session is
live.

## Syscall Surface

The following syscalls are needed, using the existing 4-scalar-argument
convention:

| Syscall | Args | Returns |
|---------|------|---------|
| `SYSCALL_SHM_ENDPOINT_CREATE` | name ptr, name len, queue count, queue depth | endpoint handle |
| `SYSCALL_SHM_ENDPOINT_OPEN` | name ptr, name len | session handle |
| `SYSCALL_SHM_SESSION_CLOSE` | session handle | status |

Concrete syscall numbers should be assigned in `user_runtime_types.h`.

## Recommended Next Steps

1. Define the concrete C ABI structs for the control page, queue header, and descriptor format.
2. Assign syscall numbers and implement the kernel syscall handlers.
3. Prototype one service on top of the transport and compare it against the existing RPC path.
