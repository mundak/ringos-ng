#include "arm64_initial_user_runtime_platform.h"

void arch_activate_process_address_space(const process* process_context)
{
  get_arm64_initial_user_runtime_platform().activate_process_address_space(process_context);
}

void arch_prepare_user_thread(const thread* thread_context)
{
  (void) thread_context;
}

[[noreturn]] void arch_run_initial_user_runtime()
{
  initial_user_runtime_platform dispatch {
    &get_arm64_initial_user_runtime_platform(),
    &initialize_arm64_platform,
    &prepare_arm64_platform,
    &enter_arm64_platform,
  };
  run_initial_user_runtime(dispatch);
}
