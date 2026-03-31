# First Platform Driver Proposal

This document records the current direction for the first real user-space
driver path in ringos-ng. The example is still the console service, but the
design is no longer "one shared driver binary plus one shared MMIO helper".

The current rule set is:

- drivers are architecture-specific
- driver sources live under `drivers/`, not under `user/`
- ordinary applications use the public SDK and RPC only
- the public SDK does not expose an MMIO mapping helper
- the kernel grants a trusted driver one architecture-defined device-memory
  window, and the driver may ask the kernel to map that assignment into its
  address space

That keeps the application-facing console contract stable while letting each
machine family choose its own hardware-facing driver implementation.

## Goals

- make the console the first real user-space service backed by a driver
- keep the client-facing console protocol shared across architectures
- keep the hardware-facing implementation architecture-specific
- move driver code under `drivers/arm64` and `drivers/x64`
- keep hardware-mapping details out of the public SDK

## Non-Goals

- a universal cross-architecture driver source file
- exposing raw MMIO concepts as a public SDK convenience API
- a full device-discovery framework in the first cut
- interrupts in the first cut

## Design Rules

### One Driver Per Architecture

The console driver is not a generic app living beside samples. Each supported
machine family gets its own driver source, build target, and hardware-facing
layout.

Current layout:

- `drivers/arm64/console_driver.c`
- `drivers/x64/console_driver.c`

The shared part is the client-visible RPC protocol, not the driver binary.

### Public SDK Boundary

The public SDK remains application-focused. It exposes:

- debug logging
- thread exit
- RPC request and reply helpers
- console protocol types

It does not expose a public device-mapping helper anymore.

Applications should not know whether a console driver talks to a UART, a
virtual transmit buffer, a debug console, or some later device model.

### Driver-Kernel Boundary

Trusted drivers use a narrower, private contract:

1. the kernel identifies which early process is the driver
2. the kernel assigns one driver-specific device-memory window to that process
3. the driver asks the kernel to map that assigned device memory
4. the driver interprets the returned region according to its architecture's
   own device expectations
5. clients talk to the driver through the assist RPC path

This keeps the kernel interface generic at the transport level while avoiding a
public SDK abstraction that bakes in one hardware-access model.

## Early Console Shape

The early console path is now split into two layers.

### Client Layer

Clients use only:

- `ringos_rpc_call()`
- `ringos_rpc_wait()`
- `ringos_rpc_reply()`
- `ringos/console.h`

Libc `puts()` and `vprintf()` stay in this layer.

### Driver Layer

Each architecture-specific driver:

- maps its assigned device memory through the raw device-memory syscall number
- interprets that region with a private layout known only to that driver
- answers `RINGOS_CONSOLE_OPERATION_GET_INFO`
- answers `RINGOS_CONSOLE_OPERATION_WRITE`

No public application header needs to know how that region is structured.

## Kernel Plumbing

The kernel side is intentionally generic.

- the runtime binds the client and driver processes to an implicit assist
  channel pair
- the runtime can assign a `device_memory_object` to the driver process
- the stage-1 device-memory syscall returns the base and size of that assigned
  window

The kernel does not need to tell the driver whether that region is "MMIO" in
SDK terms. It only needs to say: this is the device-memory assignment for your
process.

## Architecture Notes

### arm64

arm64 is the preferred first place to flesh this out further.

The current prototype still hands the driver one prearranged device-memory
region during bootstrap. That is enough to validate:

- architecture-specific driver placement
- the private driver-only mapping path
- the console RPC contract

The next arm64 step can replace the current prototype-backed region with a more
realistic hardware-specific mapping once the page-table and device policy work
is ready.

### x64

x64 keeps its own driver source and can evolve independently. That leaves room
for x64 to choose a different backend later without dragging the arm64 driver
shape with it.

The important point is that x64 no longer defines the public SDK surface for
all drivers.

## Directory Split

The repository should now be read this way:

- `user/` contains public SDK, libc, and ordinary sample applications
- `drivers/` contains trusted architecture-specific driver programs
- `arch/*/user_runtime.cpp` decides which embedded driver image a platform boots

That split keeps driver internals out of the application-facing SDK and makes
it explicit that drivers are part of platform bring-up, not generic user
samples.

## Near-Term Follow-Up

The next concrete steps are:

1. keep the console client RPC contract stable
2. flesh out arm64-specific device-memory handling further
3. add richer device-assignment metadata only when a concrete second driver
   needs it
4. avoid reintroducing public SDK helpers that expose one hardware model to all
   user-space code
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

1. The kernel boots the early trusted processes directly.
2. The platform serial driver is launched as a normal user-space process with
  only the resources it needs.
3. The kernel binds the client process and the driver process to one implicit
  assist RPC path.
4. The driver serves console requests over that assist path.
5. Client processes issue synchronous channel RPC
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

The current early path can stay even smaller than a generic inherited-
capability ABI: the kernel already knows which early process is the console
client and which is the console driver, so the first assist RPC route and MMIO
grant can be bound implicitly per process.

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

## Why x64 Should Avoid PC I/O Ports

The current x64 bring-up path uses QEMU's debug console on port `0xe9` for
kernel-host logging, but that is a debug aid rather than the right contract for
the first user-space console driver.

If the first x64 console target remains the conventional PC UART, ringos-ng has
to add a new x64-only port-I/O capability class only to let one early driver
perform `in` and `out` against legacy port space. That would make the first driver
resource model more architecture-specific before any user-space driver stack has
actually shipped.

The cleaner direction is to change the x64 console target instead of extending
the ABI around PC legacy ports:

- keep the existing `0xe9` debug console path as a kernel bring-up and
  debugger-oriented tracing mechanism
- choose a fixed MMIO-backed virtual console or UART device for the x64 driver
  path
- grant that MMIO window directly to the trusted user-space driver
- keep the client-facing console protocol identical across x64 and arm64

That keeps the first hardware-access primitive architecture-neutral without
hiding the problem in a special kernel syscall.

## Recommended Resource Model

The resource model should be generalized as device-resource handles with a kind
field rather than as unrelated one-off syscalls.

Suggested first kinds:

- `RINGOS_DEVICE_RESOURCE_MMIO_RANGE`
- `RINGOS_DEVICE_RESOURCE_IRQ_LINE`

Suggested first operations:

- map a granted MMIO range into the caller with explicit permissions and device
  memory attributes
- close the resource handle

This keeps one kernel policy model across both architectures for the first
driver cut. If a future backend genuinely needs another resource class, it
should be added for that concrete device rather than pre-committed now.

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

The first x64 serial driver should target a fixed MMIO-backed virtual console
device chosen as part of the x64 development and test platform, not the
conventional PC UART register window.

The driver responsibilities are:

- map the granted MMIO range
- initialize the device if needed
- poll transmitter readiness
- write outgoing bytes
- serve `CONSOLE_WRITE` requests over a channel

The existing port `0xe9` debug console can remain available for kernel
bring-up, but normal console service traffic should not depend on it.

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

1. place a message in the shared request buffer
2. issue `CONSOLE_WRITE` over the implicit assist RPC path
3. return success or failure

This validates the intended steady-state model for future applications.

## Staged Implementation Plan

1. Add the first RPC syscall and SDK wrapper.
2. Add MMIO mapping support for driver resource handles.
3. Fix the x64 development platform on a user-space-accessible MMIO-backed
  console device.
4. Implement the serial console driver on x64 and arm64 behind one RPC
   contract.
5. Add a sample app that writes through the console service.
6. Retarget libc `puts` to the service once the sample path is stable.

## Recommendation

The recommended direction is:

- keep the client-facing console API entirely channel-based
- treat driver space as ordinary user space with stronger capabilities
- introduce a startup info block before adding more services
- use MMIO device-resource handles as the first kernel-facing abstraction
- keep the x64 `0xe9` debug console only as a kernel bring-up path

If the project wants the smallest possible capability model, the right move is
to change the first x64 console target rather than carry legacy PC port I/O
into the driver ABI. A fixed MMIO-backed console device on both architectures
keeps the hardware boundary simple while preserving the same user-space service
model.
