#pragma once

// Enter the first architecture-specific Stage 1 user-task proof path.
// This function never returns; on unsupported targets it parks after logging.
[[noreturn]] void arch_run_initial_user_task();