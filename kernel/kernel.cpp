#include "kernel.h"

#include "boot_info.h"
#include "debug.h"
#include "panic.h"

[[noreturn]] void kernel_main(const boot_info& info)
{
  if (info.m_arch_id == ARCH_X64)
  {
    debug_semihost_log("ringos x64");
  }
  else if (info.m_arch_id == ARCH_ARM64)
  {
    debug_semihost_log("ringos arm64");
  }
  else
  {
    panic("unknown architecture id");
  }

  debug_semihost_log("gdb hooks ready");
  debug_semihost_log("hello world");

  while (true)
  {
  }
}
