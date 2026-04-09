# System Interoperability

This document defines the long-term execution and compatibility direction for
ringos-ng as it grows beyond the current native proof paths.

The project has two distinct goals:

- run ringos-native applications built with the ringos C SDK and custom Clang
  toolchain
- run unmodified Windows executables when the required compatibility surface is
  available

Those goals require the system design to separate instruction-set execution from
operating-system compatibility.

## Compatibility Axes

Every user process should be described along these independent axes:

- host architecture: the CPU architecture running the ringos kernel
- guest architecture: the CPU architecture targeted by the executable image
- process personality: the user-space contract expected by the executable, such
  as ringos-native or Windows-compatible
- execution backend: native execution, interpretation, or translation

Examples:

- ringos-native x64 program on x64 host using native execution
- ringos-native x64 program on arm64 host using translation
- Windows x64 program on x64 host using native execution
- Windows x64 program on arm64 host using translation

This split is fundamental. Guest ISA support and guest OS compatibility must not
be treated as the same problem.

## Binary Classes

ringos-ng is expected to support two main executable classes.

### Ringos-Native Binaries

These are programs built specifically for ringos-ng using the project SDK and a
ringos-aware toolchain.

They should target the ringos ABI directly and use ringos kernel services
without any Windows compatibility layer.

### Windows-Compatible Binaries

These are ordinary Windows executables that were not rebuilt for ringos-ng.

They should run through a Windows-compatible user-space environment that
provides the loader, process environment, and API surface they expect.

## Architectural Layers

The execution model should be split into separate layers.

### Kernel Layer

The kernel should remain small and architecture-neutral where possible. Its
responsibilities include:

- process and thread management
- address spaces and memory objects
- kernel object and handle management
- IPC and synchronization primitives
- scheduling and exception delivery
- canonical kernel service entry points

The kernel should not become a Windows NT compatibility implementation.

### Device And Driver Model

Device drivers should be modeled as machine-specific integrations rather than as
fully generic cross-machine components.

Some generic drivers may appear later, but the working assumption should be that
each driver is responsible for one concrete real device on one concrete machine
target, for example `qemu_arm64_virt_uart`.

Known device types should eventually be described by headers under
`kernel/include/devices/`.

Each such header should define the canonical contract for one device type,
including:

- its unique device type identifier
- the RPC calls exposed by drivers of that type
- the argument structures required by those RPC calls
- which calls are mandatory versus optional

These headers should be the shared ABI contract used by the kernel, the driver,
and user-space clients.

After successful device discovery and after the kernel loads the corresponding
driver, that driver should register itself in a global device registry or
device-manager style table.

That registration should include at least:

- the device type
- a unique device instance string
- the kernel-visible RPC endpoint or equivalent connection target for that
   device

The kernel should then expose a generic query syscall that allows user space
to:

- enumerate devices by device type
- identify a specific device instance by its unique string
- open RPC communication with the selected device driver

The expected user-space flow should be:

1. query for devices of the required type
2. choose the desired device instance by unique string
3. open an RPC connection to that driver through the kernel
4. invoke the operations defined by the corresponding device-type header under
    `kernel/include/devices/`

This keeps discovery and connection generic at the kernel boundary while
leaving device-specific behavior in explicit RPC contracts owned by each device
type.

### Execution Layer

The execution layer is responsible for running guest machine code.

Depending on the host or guest combination, a process may run:

- natively on matching hardware
- under an interpreter
- through a translated or JIT-compiled backend

This layer owns guest CPU state, instruction decoding, trap interception, and
guest memory access rules.

### Personality Layer

The personality layer provides the user-space contract expected by the program.

For ringos-native programs, this means the ringos process model, runtime, and
SDK-facing ABI.

For Windows-compatible programs, this means a Windows-like user-space
environment that can provide:

- PE image loading and relocation support
- DLL loading and import resolution
- process environment setup such as PEB and TEB style structures
- Win32 or NT user-mode API shims implemented on top of ringos facilities

## Windows Compatibility Boundary

The Windows compatibility layer should live in user space, not in kernel space.

That is a design rule, not an optimization.

The kernel should expose ringos mechanisms. The Windows personality should map
Windows application expectations onto those mechanisms from user space.

This keeps the kernel:

- smaller
- more portable across host architectures
- independent from Windows-specific policy and API growth
- reusable by both ringos-native and Windows-compatible processes

In practical terms, ringos-ng should prefer implementing Windows-facing support
as user-space components such as:

- a Windows-compatible loader
- compatibility DLLs
- subsystem services that translate Windows semantics into ringos primitives

## Canonical Process Model

The process model should be broad enough to support both native and compatible
execution.

At minimum, process creation should eventually capture:

- executable image format
- guest architecture
- process personality
- initial address-space layout
- execution backend selection
- initial thread state

Thread state should distinguish between:

- host execution context used by the kernel scheduler
- guest CPU context used by the active execution backend

This is necessary because a translated guest thread is not just a native thread
with a different entry point. It carries full guest register state and guest
fault semantics.

## Translation Model

For cross-architecture execution, the first stop should be user-mode ISA
translation.

For example, x64-on-arm64 support should:

- load an x64 guest image
- execute x64 instructions through an arm64-hosted backend
- intercept guest traps, faults, and exits
- convert guest service requests into canonical ringos kernel requests
- resume guest execution after the request completes

The kernel service boundary should stay architecture-neutral even when the guest
instruction set is not.

## Emulation Architecture

The emulation design should be broad enough to support any guest ISA on any
host ISA over time, even though the first concrete target is x64 guest code on
arm64 host.

That implies a layered model rather than a one-off x64-on-arm64 path.

### Execution Backends

Each process should run through a selected execution backend.

Examples include:

- native x64 backend
- native arm64 backend
- x64-on-arm64 interpreter backend
- x64-on-arm64 translated backend

The backend is responsible for:

- loading or binding guest CPU state for the thread
- executing guest instructions until a stop condition is reached
- surfacing guest traps, faults, and exits to shared kernel logic
- resuming execution after kernel or subsystem work completes

This allows native and emulated execution to share the same process model.

### Guest CPU State

Translated execution requires full guest CPU state that is separate from the
host thread context.

At a minimum, a translated thread should carry:

- guest instruction pointer
- guest stack pointer
- guest flags or status register state
- guest general-purpose register file
- backend-specific execution metadata such as block-cache state or decode state

The host scheduler should treat this as ordinary thread-owned execution state,
but it must not confuse guest register state with the host trap frame or native
kernel entry context.

### Canonical Service Gateway

The current native proof paths use architecture-specific trap instructions and
register conventions. That is fine for hardware entry, but it is too narrow to
serve as the long-term compatibility boundary.

The long-term design should convert every service request into a canonical
kernel-facing request frame.

That canonical frame should eventually capture:

- guest architecture
- process personality
- service or syscall number
- scalar argument registers
- guest instruction pointer at the trap point
- guest stack pointer at the trap point
- guest flags or processor state relevant to return behavior

This is required because a translated guest syscall must not depend on the host
CPU being able to execute the guest trap instruction directly.

### Emulated Trap Flow

For translated execution, guest trap instructions should be intercepted by the
backend rather than executed directly on the host CPU.

The flow should be:

1. Decode and execute guest instructions.
2. Stop when the guest reaches a trap, exit, breakpoint, fault, or unsupported
    instruction.
3. Build a canonical request frame from guest register state.
4. Dispatch that request into ringos kernel services or into the active
    user-space personality layer.
5. Write return values back into guest registers.
6. Resume guest execution at the correct next instruction.

This is the core execution loop for x64 guest code on arm64 host and for later
cross-architecture combinations.

### Memory Model

The emulation design should not assume that guest virtual addresses always equal
host virtual addresses.

Instead, each process should be able to describe:

- guest-visible virtual mappings
- host backing pages or host address mappings
- access permissions
- translation helpers used by the active execution backend

For early proof paths it may be acceptable to keep the mapping simple, but the
architecture should not depend on guest and host addresses being identical.

### Image Loading And Execution Separation

Executable loading and instruction execution should remain separate concerns.

Image loaders should parse specific file formats such as PE64 or ELF64 and
produce a normalized loaded-image description containing:

- section layout
- memory permissions
- entry point
- relocation information
- import or export metadata as needed
- stack reservation or startup metadata

Execution backends should consume that normalized result instead of embedding
format-specific loader logic inside the CPU translation path.

### Fault Model

The system should distinguish clearly between guest faults and host faults.

- a guest invalid opcode, bad guest address access, or guest breakpoint is a
   user-process event
- a host fault while interpreting or translating guest code is a kernel bug or
   backend defect

This separation matters for reliability, debugging, and later security review.

## First Emulation Scope

The first practical emulation milestone should stay intentionally small.

For x64-on-arm64, the initial scope should be:

- one guest ISA: x64
- one host ISA: arm64
- one process at a time in the proof path
- one initial user thread
- scalar argument service calls only
- interpreter-first implementation before any JIT work

The first translated workload should preferably be the existing minimal x64 user
image already used by the native x64 proof path.

That allows ringos-ng to prove:

- x64 guest image load on arm64 host
- guest instruction execution through the backend
- service-call interception and return
- clean guest thread exit

before introducing Windows personality concerns.

## Windows Compatibility And Emulation

Windows compatibility and ISA emulation should compose cleanly.

That means:

- a Windows x64 process on x64 host can use a native x64 backend with a Windows
   user-space personality
- that same Windows x64 process on arm64 host can use an x64-on-arm64 backend
   with the same Windows user-space personality

This is why the Windows compatibility layer must not be fused with host-CPU
details.

The preferred long-term direction is:

- keep the kernel focused on ringos mechanisms
- implement Windows-facing behavior in user-space loaders, DLLs, and subsystem
   services
- allow those user-space components to run either natively or under guest-ISA
   translation depending on the process configuration

## Windows Bring-Up Strategy

The first Windows milestone should not require ISA emulation.

The preferred order is:

1. Load and run a simple Windows x64 executable on x64 host.
2. Provide the minimum Windows-compatible user-space environment needed for
    that process.
3. Validate that the process model, loader, and personality split are sound.
4. Reuse the same Windows x64 environment under the x64-on-arm64 execution
    backend.

This isolates Windows personality work from ISA translation work and makes both
systems easier to debug.

## Recommended Milestones

The likely implementation order is:

1. Generalize the process model to include guest architecture, personality, and
   execution backend.
2. Define a canonical kernel request or syscall frame that is independent from
   any one trap instruction.
3. Separate image loading from execution so PE and other formats can feed a
   normalized process bootstrap path.
4. Bring up ringos-native cross-architecture execution, starting with x64 guest
   code on arm64 host.
5. Bring up a Windows-compatible x64 process on x64 host first, where no ISA
   translation is needed.
6. Run that same Windows-compatible x64 environment on arm64 through the x64
   translation backend.

This order keeps ISA translation and Windows compatibility as separate problems
until both are individually understood.

## Non-Goals For The First Cut

The first implementation should not try to solve every compatibility problem at
once.

Early stages should avoid assuming support for:

- arbitrary guest architectures on day one
- full Win32 coverage
- dynamic binary translation for every workload immediately
- kernel-mode Windows driver compatibility
- direct cloning of the Windows kernel ABI inside ringos-ng

The first successful milestone is smaller: a well-structured execution model
that can host both ringos-native programs and a future user-space Windows
compatibility subsystem without redesigning the kernel.
