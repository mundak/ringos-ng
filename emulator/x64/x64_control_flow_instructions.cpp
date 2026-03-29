#include "x64_execution_context.h"
#include "x64_instruction_handlers.h"

namespace
{
  constexpr uint64_t RFLAGS_ZERO = 1ULL << 6;

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

x64_instruction_outcome execute_x64_return(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return x64_instruction_outcome::stop_running;
  }

  uint64_t return_address = 0;

  if (!context.pop_u64(&return_address))
  {
    context.get_result().completion = x64_emulator_completion::invalid_memory_access;
    return x64_instruction_outcome::stop_running;
  }

  context.get_state().instruction_pointer = static_cast<uintptr_t>(return_address);
  return x64_instruction_outcome::continue_running;
}

x64_instruction_outcome execute_x64_call_relative(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return x64_instruction_outcome::stop_running;
  }

  int32_t displacement = 0;

  if (!context.read_i32(instruction.next_address, &displacement))
  {
    context.get_result().completion = x64_emulator_completion::invalid_memory_access;
    return x64_instruction_outcome::stop_running;
  }

  const uintptr_t return_address = instruction.next_address + sizeof(int32_t);

  if (!context.push_u64(return_address))
  {
    context.get_result().completion = x64_emulator_completion::invalid_memory_access;
    return x64_instruction_outcome::stop_running;
  }

  context.get_state().instruction_pointer = return_address + displacement;
  return x64_instruction_outcome::continue_running;
}

x64_instruction_outcome execute_x64_jump_relative_near(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return x64_instruction_outcome::stop_running;
  }

  int32_t displacement = 0;

  if (!context.read_i32(instruction.next_address, &displacement))
  {
    context.get_result().completion = x64_emulator_completion::invalid_memory_access;
    return x64_instruction_outcome::stop_running;
  }

  context.get_state().instruction_pointer = instruction.next_address + sizeof(int32_t) + displacement;
  return x64_instruction_outcome::continue_running;
}

x64_instruction_outcome execute_x64_jump_relative_short(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return x64_instruction_outcome::stop_running;
  }

  int8_t displacement = 0;

  if (!context.read_i8(instruction.next_address, &displacement))
  {
    context.get_result().completion = x64_emulator_completion::invalid_memory_access;
    return x64_instruction_outcome::stop_running;
  }

  context.get_state().instruction_pointer = instruction.next_address + sizeof(int8_t) + displacement;
  return x64_instruction_outcome::continue_running;
}

x64_instruction_outcome execute_x64_jump_short_condition(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return x64_instruction_outcome::stop_running;
  }

  int8_t displacement = 0;

  if (!context.read_i8(instruction.next_address, &displacement))
  {
    context.get_result().completion = x64_emulator_completion::invalid_memory_access;
    return x64_instruction_outcome::stop_running;
  }

  const bool zero_flag_set = (context.get_state().flags & RFLAGS_ZERO) != 0;
  const bool take_branch = instruction.opcode == 0x74 ? zero_flag_set : !zero_flag_set;
  context.get_state().instruction_pointer = take_branch ? instruction.next_address + sizeof(int8_t) + displacement
                                                        : instruction.next_address + sizeof(int8_t);
  return x64_instruction_outcome::continue_running;
}

