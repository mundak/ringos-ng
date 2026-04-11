#pragma once

#include "boot_info.h"

// Shared kernel entry point. Both architectures must call this function
// after completing their architecture-specific startup sequence.
// Never returns.
[[noreturn]] void kernel_main(const boot_info& info);
