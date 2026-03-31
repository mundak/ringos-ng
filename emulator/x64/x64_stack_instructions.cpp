#include "x64_execution_context.h"
#include "x64_instruction_handlers.h"

namespace
{
  bool reject_rex_w(x64_execution_context& context, const x64_decoded_instruction& instruction)
  {
    if (!instruction.rex_w)
    {
      return false;
    }

    context.set_unsupported_instruction(instruction.opcode);
    return true;
  }
}

x64_instruction_outcome execute_x64_push_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint32_t register_index = (instruction.opcode - 0x50) + (instruction.rex_b ? 8U : 0U);
  const uint64_t value = context.get_register64(register_index);

  if (!context.push_u64(value))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.get_state().instruction_pointer = instruction.next_address;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_pop_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint32_t register_index = (instruction.opcode - 0x58) + (instruction.rex_b ? 8U : 0U);
  uint64_t value = 0;

  if (!context.pop_u64(&value))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.get_register64(register_index) = value;
  context.get_state().instruction_pointer = instruction.next_address;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

