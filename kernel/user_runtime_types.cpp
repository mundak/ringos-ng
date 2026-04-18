#include "user_runtime_types.h"

user_syscall_context make_user_syscall_context(
  uint64_t syscall_number,
  uintptr_t trap_instruction_pointer,
  uintptr_t stack_pointer,
  uintptr_t trap_flags,
  uint64_t argument0,
  uint64_t argument1,
  uint64_t argument2,
  uint64_t argument3,
  uint64_t argument4,
  uint64_t argument5)
{
  return {
    syscall_number, trap_instruction_pointer,
    stack_pointer,  trap_flags,
    argument0,      argument1,
    argument2,      argument3,
    argument4,      argument5,
  };
}

thread_context make_thread_context_from_syscall(const user_syscall_context& syscall_context)
{
  return {
    syscall_context.trap_instruction_pointer,
    syscall_context.stack_pointer,
    syscall_context.trap_flags,
    static_cast<uintptr_t>(syscall_context.argument0),
  };
}
