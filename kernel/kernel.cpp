#include "kernel.h"

#include "arch_user_runtime.h"
#include "boot_info.h"
#include "debug.h"
#include "machine.h"
#include "panic.h"

[[noreturn]] void kernel_main(const boot_info& info)
{
  initialize_machine(info);

  if (info.arch_id == ARCH_X64)
  {
    debug_log("ringos x64");
  }
  else if (info.arch_id == ARCH_ARM64)
  {
    debug_log("ringos arm64");
  }
  else
  {
    panic("unknown architecture id");
  }

  debug_log("gdb hooks ready");
  debug_log("hello world");

  arch_run_initial_user_runtime();
}
