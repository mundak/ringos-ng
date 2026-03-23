#pragma once

namespace ringos
{

  // Write a null-terminated string to the serial console.
  // Implemented by each architecture's platform layer (Phase 3 for x64,
  // Phase 4 for arm64). Must be safe to call immediately after the
  // architecture-specific console initialization has completed.
  void console_write(const char* str);

} // namespace ringos
