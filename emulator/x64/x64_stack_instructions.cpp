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
    return x64_instruction_outcome::stop_running;
  }

  const uint32_t register_index = (instruction.opcode - 0x50) + (instruction.rex_b ? 8U : 0U);
  const uint64_t value = context.get_register64(register_index);

  if (!context.push_u64(value))
  {
    context.get_result().completion = x64_emulator_completion::invalid_memory_access;
    return x64_instruction_outcome::stop_running;
  }

  context.get_state().instruction_pointer = instruction.next_address;
  return x64_instruction_outcome::continue_running;
}

x64_instruction_outcome execute_x64_pop_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return x64_instruction_outcome::stop_running;
  }

  const uint32_t register_index = (instruction.opcode - 0x58) + (instruction.rex_b ? 8U : 0U);
  uint64_t value = 0;

  if (!context.pop_u64(&value))
  {
    context.get_result().completion = x64_emulator_completion::invalid_memory_access;
    return x64_instruction_outcome::stop_running;
  }

  context.get_register64(register_index) = value;
  context.get_state().instruction_pointer = instruction.next_address;
  return x64_instruction_outcome::continue_running;
}
