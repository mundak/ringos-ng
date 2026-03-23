// Default no-op implementation of console_write. This weak definition is
// overridden by each architecture's platform-specific serial driver once it
// is implemented (Phase 3 for x64, Phase 4 for arm64).

#include "console.h"

namespace ringos
{

  [[gnu::weak]] void console_write(const char* str)
  {
    (void)str;
  }

} // namespace ringos
