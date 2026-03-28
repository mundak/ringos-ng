#include "kernel.h"

#include "boot_info.h"
#include "console.h"
#include "debug.h"
#include "panic.h"

[[noreturn]] void kernel_main(const boot_info& info)
{
  if (info.m_arch_id == ARCH_X64)
  {
    console_write("ringos x64\n");
  }
  else if (info.m_arch_id == ARCH_ARM64)
  {
    console_write("ringos arm64\n");
  }
  else
  {
    panic("unknown architecture id");
  }

  debug_log("gdb hooks ready");
  console_write("hello world\n");

  while (true)
  {
  }
}
