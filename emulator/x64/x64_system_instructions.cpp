#include "x64_execution_context.h"
#include "x64_instruction_handlers.h"

x64_instruction_outcome execute_x64_unsupported(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  context.set_unsupported_instruction(instruction.opcode);
  return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
}

x64_instruction_outcome execute_x64_syscall(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  bool should_continue = false;
  context.get_state().instruction_pointer = instruction.next_address + 1;

  const int32_t syscall_result
    = context.get_callbacks().handle_syscall(context.get_callbacks().context, context.get_state(), &should_continue);
  context.get_register64(static_cast<uint32_t>(X64_GENERAL_REGISTER_RAX))
    = static_cast<uint64_t>(static_cast<int64_t>(syscall_result));

  if (!should_continue)
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_THREAD_EXITED;
    return X64_INSTRUCTION_OUTCOME_RETIRE_AND_STOP;
  }

  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}
