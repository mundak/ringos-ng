#include "x64_execution_context.h"
#include "x64_instruction_handlers.h"

x64_instruction_outcome execute_x64_xor_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (instruction.rex_w)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return x64_instruction_outcome::STOP_RUNNING;
  }

  uint8_t modrm = 0;

  if (!context.read_u8(instruction.next_address, &modrm))
  {
    context.get_result().completion = x64_emulator_completion::INVALID_MEMORY_ACCESS;
    return x64_instruction_outcome::STOP_RUNNING;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint8_t source_register = static_cast<uint8_t>((modrm >> 3) & 0x7);
  const uint8_t destination_register = static_cast<uint8_t>(modrm & 0x7);

  if (mod != 3)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return x64_instruction_outcome::STOP_RUNNING;
  }

  const uint32_t result_value = context.get_register32(destination_register) ^ context.get_register32(source_register);
  context.set_register32(destination_register, result_value);
  context.set_logic_flags(result_value, false);
  context.get_state().instruction_pointer = instruction.next_address + 1;
  return x64_instruction_outcome::CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_test_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uint8_t modrm = 0;

  if (!context.read_u8(instruction.next_address, &modrm))
  {
    context.get_result().completion = x64_emulator_completion::INVALID_MEMORY_ACCESS;
    return x64_instruction_outcome::STOP_RUNNING;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint8_t source_register = static_cast<uint8_t>((modrm >> 3) & 0x7);
  const uint8_t destination_register = static_cast<uint8_t>(modrm & 0x7);

  if (mod != 3)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return x64_instruction_outcome::STOP_RUNNING;
  }

  if (instruction.rex_w)
  {
    const uint64_t result_value
      = context.get_register64(destination_register) & context.get_register64(source_register);
    context.set_logic_flags(result_value, true);
  }
  else
  {
    const uint32_t result_value
      = context.get_register32(destination_register) & context.get_register32(source_register);
    context.set_logic_flags(result_value, false);
  }

  context.get_state().instruction_pointer = instruction.next_address + 1;
  return x64_instruction_outcome::CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_group83(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uint8_t modrm = 0;
  int8_t immediate = 0;

  if (!context.read_u8(instruction.next_address, &modrm) || !context.read_i8(instruction.next_address + 1, &immediate))
  {
    context.get_result().completion = x64_emulator_completion::INVALID_MEMORY_ACCESS;
    return x64_instruction_outcome::STOP_RUNNING;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint8_t operation = static_cast<uint8_t>((modrm >> 3) & 0x7);
  const uint8_t register_index = static_cast<uint8_t>(modrm & 0x7);

  if (mod != 3)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return x64_instruction_outcome::STOP_RUNNING;
  }

  if (instruction.rex_w)
  {
    const uint64_t lhs = context.get_register64(register_index);
    const uint64_t rhs = static_cast<uint64_t>(static_cast<int64_t>(immediate));

    if (operation == 0)
    {
      const uint64_t sum = lhs + rhs;
      context.get_register64(register_index) = sum;
      context.set_add_flags(lhs, rhs, sum, true);
    }
    else if (operation == 5)
    {
      const uint64_t difference = lhs - rhs;
      context.get_register64(register_index) = difference;
      context.set_compare_flags(lhs, rhs, difference, true);
    }
    else if (operation == 7)
    {
      const uint64_t difference = lhs - rhs;
      context.set_compare_flags(lhs, rhs, difference, true);
    }
    else
    {
      context.set_unsupported_instruction(instruction.opcode);
      return x64_instruction_outcome::STOP_RUNNING;
    }
  }
  else
  {
    const uint32_t lhs = context.get_register32(register_index);
    const uint32_t rhs = static_cast<uint32_t>(static_cast<int32_t>(immediate));

    if (operation == 0)
    {
      const uint32_t sum = lhs + rhs;
      context.set_register32(register_index, sum);
      context.set_add_flags(lhs, rhs, sum, false);
    }
    else if (operation == 5)
    {
      const uint32_t difference = lhs - rhs;
      context.set_register32(register_index, difference);
      context.set_compare_flags(lhs, rhs, difference, false);
    }
    else if (operation == 7)
    {
      const uint32_t difference = lhs - rhs;
      context.set_compare_flags(lhs, rhs, difference, false);
    }
    else
    {
      context.set_unsupported_instruction(instruction.opcode);
      return x64_instruction_outcome::STOP_RUNNING;
    }
  }

  context.get_state().instruction_pointer = instruction.next_address + 2;
  return x64_instruction_outcome::CONTINUE_RUNNING;
}
