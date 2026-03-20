# ringos-ng

This repository is planned as a bare-metal operating system project targeting multiple CPU architectures.

## Current Scope

The current approved target is milestone one only:

- Boot a minimal kernel in QEMU on x64.
- Boot a minimal kernel in QEMU virt on arm64.
- Reach a shared C++ kernel entry path on both targets.
- Print hello world over serial on both targets.
- Build and run from WSL2 on Windows.

## Milestone One Plan

The detailed implementation plan is documented in [docs/milestone-one-execution-plan.md](docs/milestone-one-execution-plan.md).
The recommended CI and local testing structure is documented in [docs/ci-and-testing.md](docs/ci-and-testing.md).

That document is the current source of truth for:

- scope and non-goals
- execution phases
- architecture boundaries
- verification gates
- handoff expectations for the next implementation agent

## Milestone One Non-Goals

These are explicitly out of scope for the first milestone:

- SMP
- interrupts
- timers
- scheduler
- userspace
- filesystem or storage
- framebuffer output
- real hardware support
- advanced MMU work beyond what is strictly needed for x64 kernel entry

## Development Environment

- Host OS: Windows
- Primary build and debug environment: WSL2
- Language after early bootstrap: C++
- Expected early-bootstrap language: assembly
- Initial emulation targets: QEMU x64 and QEMU arm64 virt

## Status

Implementation has not started yet. The next step is to create the initial repository skeleton and begin phase 1 from the milestone-one execution plan.
