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
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  uint64_t return_address = 0;

  if (!context.pop_u64(&return_address))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.get_state().instruction_pointer = static_cast<uintptr_t>(return_address);
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_call_relative(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  int32_t displacement = 0;

  if (!context.read_i32(instruction.next_address, &displacement))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uintptr_t return_address = instruction.next_address + sizeof(int32_t);

  if (!context.push_u64(return_address))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.get_state().instruction_pointer = return_address + displacement;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_call_indirect_memory(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uint8_t modrm = 0;

  if (!context.read_u8(instruction.next_address, &modrm))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint8_t operation = static_cast<uint8_t>((modrm >> 3) & 0x7);
  const uint8_t rm = static_cast<uint8_t>(modrm & 0x7);

  if (operation != 2)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  uintptr_t target_address = 0;
  uintptr_t return_address = instruction.next_address + 1;

  if (mod == 3)
  {
    target_address = static_cast<uintptr_t>(context.get_register64(rm));
  }
  else if (mod == 0 && rm == 5)
  {
    int32_t displacement = 0;

    if (!context.read_i32(instruction.next_address + 1, &displacement))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    const uintptr_t memory_address = instruction.next_address + 5 + displacement;
    uint64_t loaded_target = 0;

    if (!context.read_u64(memory_address, &loaded_target))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    target_address = static_cast<uintptr_t>(loaded_target);
    return_address = instruction.next_address + 5;
  }
  else
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  if (!context.push_u64(return_address))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.get_state().instruction_pointer = target_address;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_jump_relative_near(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  int32_t displacement = 0;

  if (!context.read_i32(instruction.next_address, &displacement))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.get_state().instruction_pointer = instruction.next_address + sizeof(int32_t) + displacement;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_jump_relative_short(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  int8_t displacement = 0;

  if (!context.read_i8(instruction.next_address, &displacement))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.get_state().instruction_pointer = instruction.next_address + sizeof(int8_t) + displacement;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_jump_short_condition(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  int8_t displacement = 0;

  if (!context.read_i8(instruction.next_address, &displacement))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const bool zero_flag_set = (context.get_state().flags & RFLAGS_ZERO) != 0;
  const bool take_branch = instruction.opcode == 0x74 ? zero_flag_set : !zero_flag_set;
  context.get_state().instruction_pointer = take_branch ? instruction.next_address + sizeof(int8_t) + displacement
                                                        : instruction.next_address + sizeof(int8_t);
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

