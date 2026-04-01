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

- `drivers/arm64/console_driver.cpp`
- `drivers/x64/console_driver.cpp`

The shared part is the client-visible RPC protocol, not the driver binary.

### Public SDK Boundary

The public SDK remains application-focused. It exposes:

- debug logging
- thread exit
- named RPC endpoint open helpers
- console-device enumeration helpers
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

- `ringos_console_query_devices()`
- `ringos_rpc_open()`
- `ringos_console_get_info()`
- `ringos_console_write()`
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
- the SDK enumerates console RPC endpoints and opens them by name
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
