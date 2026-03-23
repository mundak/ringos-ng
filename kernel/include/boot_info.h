#pragma once

#include <stdint.h>

namespace ringos
{

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
  //   - The serial console is ready to accept writes.
  //
  // Runtime restrictions in kernel code:
  //   - No C++ exceptions.
  //   - No RTTI.
  //   - No libc dependency.
  //   - No dynamic memory allocation.
  //   - No reliance on implicit global constructors.
  struct boot_info
  {
    uint32_t m_arch_id; // One of the ARCH_* constants defined above.
    uint32_t m_reserved0; // Reserved. Must be zero.
    uint64_t m_reserved1; // Reserved. Must be zero.
    uint64_t m_reserved2; // Reserved. Must be zero.
    uint64_t m_reserved3; // Reserved. Must be zero.
    uint64_t m_reserved4; // Reserved. Must be zero.
  };

}
