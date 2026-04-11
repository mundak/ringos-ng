#pragma once

#include <stddef.h>
#include <stdint.h>

static constexpr uint32_t ARCH_X64 = 0;
static constexpr uint32_t ARCH_ARM64 = 1;

// Boot handoff structure populated by each architecture's startup code
// before calling kernel_main. This structure is the stable ABI boundary
// between the architecture-specific boot path and the shared kernel.
//
// Preconditions when kernel_main is entered:
//   - Stack is valid and sized for kernel use.
//   - BSS has been zeroed.
//   - CPU is in the intended operating mode (64-bit long mode for x64,
//     EL1 for arm64).
//   - The selected launcher has configured the host-side debug log sink.
//
// Runtime restrictions in kernel code:
//   - No C++ exceptions.
//   - No RTTI.
//   - No libc dependency.
//   - No dynamic memory allocation.
//   - No reliance on implicit global constructors.
struct boot_info
{
  uint32_t arch_id; // One of the ARCH_* constants defined above.
  uintptr_t device_tree_blob_address;
  size_t device_tree_blob_size;
};
