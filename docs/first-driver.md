# First User-Space Driver Proposal

This document proposes the first real user-space driver path for ringos-ng.
The concrete example is a serial console service that works on both x64 and
arm64 while preserving the current microkernel direction: the kernel exposes
mechanisms, while drivers and higher-level services live in user space.

The immediate goal is to replace the current special-case user console path
with a real service model that can later support stdout, stderr, logging, and
other device-backed streams without adding more ad hoc kernel syscalls.

## Goals

- make the serial console the first user-space driver-backed service
- use the public SDK and the channel RPC model rather than a console-specific
  kernel ABI
- keep one client-facing console protocol across x64 and arm64
- define the minimum process-launch and resource-capability APIs needed to
  launch trusted drivers
- create a sample app that writes to the console service through the same
  public interface that libc can later use

## Non-Goals

- full interrupt-driven serial I/O in the first cut
- a general PCI bus manager in the first cut
- a stable long-term device-discovery standard for every future subsystem
- conflating application processes and driver processes into different runtime
  models

Driver space is still user space. The difference is capability, not process
kind.

## Design Direction

The intended split is:

- the kernel owns protection, scheduling, handle management, channels, shared
  memory, and resource mapping
- trusted user-space drivers own hardware-facing register access
- ordinary applications talk to services over channels

That gives ringos-ng one service model for both drivers and applications.
Normal applications should not know whether the console is backed by a UART, a
debug sink, or something else. They only know about a console service handle
and a versioned RPC contract.

## Why The First Driver Should Be Serial Console

Serial console is a good first driver because it is narrow, testable, and
already useful:

- both targets already have a concept of host-visible debug output
- the protocol surface is small enough to validate the driver architecture
- libc `puts` can later be redirected to it cleanly
- the sample app can prove the entire stack end to end

The first implementation should stay polling-only. Interrupt delivery can be a
later extension once the resource and RPC model exists.

## Kernel And User-Space Split

The first serial stack should look like this:

1. The kernel boots an init process with a startup capability block.
2. Init starts a device-manager process or performs the same role directly in
   the early cut.
3. The platform serial driver is launched as a normal user-space process with
   only the resources it needs.
4. The driver registers a console service endpoint.
5. Client processes connect to that service and issue synchronous channel RPC
   calls.

The key point is that client processes do not map UART registers directly.
Only the driver does.

## Required SDK And ABI Additions

The current SDK exposes raw syscall entry points, debug logging, and thread
exit. That is not enough for a driver-backed service model. The following
surfaces should be added next.

### Process Entry Contract

The first cut does not need a dedicated startup-info block. Keeping the entry
contract at `main(void)` is enough while the console driver work is focused on
channel RPCs and explicit resource grants.

If process metadata or inherited capabilities become necessary later, they
should be introduced only for a concrete use case rather than reserved in the
ABI up front.

### RPC Helpers

The SDK should expose a stable RPC helper that matches the documented
synchronous request-reply model.

At minimum:

- create or receive RPC endpoint handles
- perform a blocking RPC call on an RPC endpoint handle
- close RPC endpoint handles

The client-side helper should hide the register-level syscall calling
convention from service code.

### Shared Memory Helpers

The SDK should expose:

- create shared-memory objects
- map and unmap shared-memory objects
- pass mapped buffers through channel RPCs

This keeps larger request or reply payloads out of the fixed register budget.

### Driver Resource Helpers

Trusted drivers need capability-scoped access to hardware resources. The first
resource classes should be:

- MMIO ranges
- I/O port ranges
- IRQ lines later, not required for the initial polling cut

These should be granted as handles, not as ambient process privileges.

## Concrete SDK v0 Sketch

The first implementation pass should introduce these public SDK types:

- `ringos_rpc_request` and `ringos_rpc_response` in `ringos/rpc.h`
- `ringos_console_get_info_response`, `ringos_console_write_request`, and
  `ringos_console_write_response` in `ringos/console.h`

Suggested initial values:

- `RINGOS_SYSCALL_RPC_CALL = 3`
- `RINGOS_CONSOLE_OPERATION_GET_INFO = 1`
- `RINGOS_CONSOLE_OPERATION_WRITE = 2`
- `RINGOS_CONSOLE_PROTOCOL_VERSION = 1`

The initial generic RPC split should be:

- syscall return value: transport-level status such as bad handle or fault
- response `status` field: service-level status returned by the target server

That separation makes later service protocols easier to evolve without turning
every RPC failure into a transport failure.

## Why `ioport` Exists

The reason `ioport` appears in the proposal is not that ringos-ng should build
an architecture-wide programming model around x64 legacy ports. The reason is
much narrower: the common PC serial path on x64 is a 16550-style UART exposed
through the x86 I/O port space rather than through MMIO.

For the first x64 serial driver, if we want the driver to stay in user space,
the driver needs some way to perform controlled `in` and `out` operations on
that UART register range. Without an `ioport` resource concept, one of the
following would have to happen:

- keep the x64 UART path in the kernel while arm64 uses a user-space MMIO
  driver
- add a special x64 serial syscall that bypasses the driver model
- switch the first x64 hardware target to a different MMIO-backed console
  device instead of the conventional PC UART

The first two options undercut the point of proving a real user-space driver
boundary. The third option is viable, but it means changing the platform plan
rather than simplifying the capability model.

So the proposal uses `ioport` for one reason: it is the mechanism that lets a
user-space x64 serial driver control the standard PC UART without leaving the
microkernel design.

## Why `ioport` Should Stay Narrow

Even if ringos-ng adds `ioport`, it should stay tightly scoped.

- It is a driver capability, not an application API.
- It is architecture-specific, not part of the portable client-facing service
  contract.
- It should be range-based and least-privilege, for example COM1's register
  window only, not unrestricted access to the whole port space.
- Normal console clients should never see it.

The portable abstraction is the console RPC protocol. `ioport` is only an
implementation detail of one backend.

## Alternative To `ioport`

If the project wants to avoid `ioport`, the clean alternative is not to hide
the problem in the kernel. The clean alternative is to choose a first x64
device that is MMIO-backed on both architectures.

That would give a simpler driver-capability model:

- `map_device_memory`
- later `bind_interrupt`

The tradeoff is that the initial x64 console path would no longer match the
conventional PC serial device. It would depend on selecting or emulating a
different console device.

Recommendation:

- if the near-term goal is proving a user-space driver against the current PC
  UART path, keep `ioport`
- if the near-term goal is minimizing hardware-resource concepts even at the
  cost of changing the x64 console device, move to an MMIO-only console target

## Recommended Resource Model

The resource model should be generalized as device-resource handles with a kind
field rather than as unrelated one-off syscalls.

Suggested first kinds:

- `RINGOS_DEVICE_RESOURCE_MMIO_RANGE`
- `RINGOS_DEVICE_RESOURCE_IOPORT_RANGE`
- `RINGOS_DEVICE_RESOURCE_IRQ_LINE`

Suggested first operations:

- map a granted MMIO range into the caller with explicit permissions and device
  memory attributes
- read and write within a granted I/O port range
- close the resource handle

This preserves one kernel policy model even though x64 and arm64 need
different low-level access paths.

## MMIO Mapping Requirements

For MMIO-backed devices, the SDK should expose a dedicated mapping routine with
permission flags and device-memory attributes.

Required properties:

- readable and writable flags
- no executable device mappings
- uncached or device-ordered mapping mode
- explicit virtual-address result returned to the driver
- failure if the handle does not refer to a granted MMIO resource

The API should be specific about device memory, not reuse a future general file
or anonymous-memory mapping API without distinction.

## First Console Service Contract

The first client-facing console service should stay deliberately small.

Suggested operations:

- `CONSOLE_WRITE`
- `CONSOLE_GET_INFO`

`CONSOLE_WRITE` should take a shared-buffer pointer or shared-buffer descriptor
plus length and return a status code and the number of bytes accepted.

`CONSOLE_GET_INFO` can return:

- protocol version
- console kind
- capability flags such as write support

The protocol should be byte-oriented even if the first implementation is only
used for text.

## Service Discovery

The first cut should avoid hard-coded global handles in applications.

Instead, init should provide either:

- a namespace or name-service bootstrap channel, or
- a directly inherited console service channel for the earliest sample

Longer term, a dedicated name-service process is the better direction because
it scales to multiple services and multiple driver instances.

Suggested early service names:

- `console.default`
- `serial.primary`

## Driver Layout By Architecture

### x64

The first x64 serial driver can target the standard PC UART register window.
If that path remains the target, it requires a granted `ioport` range.

The driver responsibilities are:

- initialize the UART if needed
- poll transmitter readiness
- write outgoing bytes
- serve `CONSOLE_WRITE` requests over a channel

### arm64

The first arm64 serial driver can target the platform UART through MMIO.

The driver responsibilities are the same as x64 from the service boundary
outward:

- map the granted MMIO range
- poll transmitter readiness
- write outgoing bytes
- serve `CONSOLE_WRITE` requests over a channel

The backend differs, but the user-facing protocol stays the same.

## libc Direction

The current hosted C path uses a special debug-log escape hatch for `puts`.
That should eventually be replaced with:

- console service discovery
- `CONSOLE_WRITE` RPC
- libc buffering policy entirely in user space

That keeps libc above the SDK and above user-space services rather than turning
stdio into another kernel ABI layer.

## First Sample App

The first sample app should not use a console-specific kernel syscall.

It should:

1. obtain a console service channel from startup info or simple name lookup
2. place a message in the shared request buffer
3. issue `CONSOLE_WRITE`
4. return success or failure

This validates the intended steady-state model for future applications.

## Staged Implementation Plan

1. Freeze the process startup-info structure and the first driver resource
   handle kinds.
2. Extend crt0 and the SDK so startup capabilities are visible in user space.
3. Add the first RPC syscall and SDK wrapper.
4. Add MMIO mapping support for driver resource handles.
5. Decide whether x64 keeps the standard PC UART path.
6. If yes, add narrow `ioport` resource support for trusted user-space
   drivers.
7. Implement the serial console driver on x64 and arm64 behind one RPC
   contract.
8. Add a sample app that writes through the console service.
9. Retarget libc `puts` to the service once the sample path is stable.

## Recommendation

The recommended direction is:

- keep the client-facing console API entirely channel-based
- treat driver space as ordinary user space with stronger capabilities
- introduce a startup info block before adding more services
- use device-resource handles as the kernel-facing abstraction
- keep `ioport` only if the first x64 serial target remains the standard PC
  UART

If the project wants the smallest possible capability model, the right debate
is not whether x64 ports are aesthetically pleasing. The real question is
whether the first x64 console target should remain a legacy PC UART or move to
an MMIO-backed device so both architectures can share the same hardware-access
primitive.
