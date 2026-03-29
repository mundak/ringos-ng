#include "x64_execution_context.h"
#include "x64_instruction_handlers.h"

x64_instruction_outcome execute_x64_unsupported(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  context.set_unsupported_instruction(instruction.opcode);
  return x64_instruction_outcome::stop_running;
}

x64_instruction_outcome execute_x64_syscall(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  bool should_continue = false;
  context.get_state().instruction_pointer = instruction.next_address + 1;

  const int32_t syscall_result
    = context.get_callbacks().handle_syscall(context.get_callbacks().context, context.get_state(), &should_continue);
  context.get_register64(static_cast<uint32_t>(x64_general_register::rax))
    = static_cast<uint64_t>(static_cast<int64_t>(syscall_result));

  if (!should_continue)
  {
    context.get_result().completion = x64_emulator_completion::thread_exited;
    return x64_instruction_outcome::retire_and_stop;
  }

  return x64_instruction_outcome::continue_running;
}
