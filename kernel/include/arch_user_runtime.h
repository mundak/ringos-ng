#pragma once

class process;

// Enter the first architecture-specific user-runtime proof path.
// This function never returns.
[[noreturn]] void arch_run_initial_user_runtime();
void arch_activate_process_address_space(const process* process_context);
