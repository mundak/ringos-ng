#pragma once

#include <stdint.h>

namespace ringos
{

  static constexpr uint32_t k_arch_x64 = 0;
  static constexpr uint32_t k_arch_arm64 = 1;

  // Boot handoff structure populated by each architecture's startup code
  // before calling kernel_main. This structure is the stable ABI boundary
  // between the architecture-specific boot path and the shared kernel.
  //
  // Preconditions when kernel_main is entered:
  //   - Stack is valid and sized for kernel use.
  //   - BSS has been zeroed.
  //   - CPU is in the intended operating mode (64-bit long mode for x64,
  //     EL1 for arm64).
  //   - The serial console is ready to accept writes.
  //
  // Runtime restrictions in kernel code:
  //   - No C++ exceptions.
  //   - No RTTI.
  //   - No libc dependency.
  //   - No dynamic memory allocation.
  //   - No reliance on implicit global constructors.
  struct BootInfo
  {
    uint32_t arch_id;   // One of the k_arch_* constants defined above.
    uint32_t reserved0; // Reserved. Must be zero.
    uint64_t reserved1; // Reserved. Must be zero.
    uint64_t reserved2; // Reserved. Must be zero.
    uint64_t reserved3; // Reserved. Must be zero.
    uint64_t reserved4; // Reserved. Must be zero.
  };

} // namespace ringos
