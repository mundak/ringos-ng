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

x64_instruction_outcome execute_x64_mov_immediate32(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return x64_instruction_outcome::stop_running;
  }

  const uint32_t register_index = (instruction.opcode - 0xB8) + (instruction.rex_b ? 8U : 0U);
  uint32_t immediate = 0;

  if (!context.read_u32(instruction.next_address, &immediate))
  {
    context.get_result().completion = x64_emulator_completion::invalid_memory_access;
    return x64_instruction_outcome::stop_running;
  }

  context.set_register32(register_index, immediate);
  context.get_state().instruction_pointer = instruction.next_address + sizeof(uint32_t);
  return x64_instruction_outcome::continue_running;
}

x64_instruction_outcome execute_x64_mov_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uint8_t modrm = 0;

  if (!context.read_u8(instruction.next_address, &modrm))
  {
    context.get_result().completion = x64_emulator_completion::invalid_memory_access;
    return x64_instruction_outcome::stop_running;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint8_t source_register = static_cast<uint8_t>((modrm >> 3) & 0x7);
  const uint8_t destination_register = static_cast<uint8_t>(modrm & 0x7);

  if (mod != 3)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return x64_instruction_outcome::stop_running;
  }

  if (instruction.rex_w)
  {
    context.get_register64(destination_register) = context.get_register64(source_register);
  }
  else
  {
    context.set_register32(destination_register, context.get_register32(source_register));
  }

  context.get_state().instruction_pointer = instruction.next_address + 1;
  return x64_instruction_outcome::continue_running;
}

x64_instruction_outcome execute_x64_lea_rip_relative(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (!instruction.rex_w)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return x64_instruction_outcome::stop_running;
  }

  uint8_t modrm = 0;
  int32_t displacement = 0;

  if (
    !context.read_u8(instruction.next_address, &modrm)
    || !context.read_i32(instruction.next_address + 1, &displacement))
  {
    context.get_result().completion = x64_emulator_completion::invalid_memory_access;
    return x64_instruction_outcome::stop_running;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint8_t register_index = static_cast<uint8_t>((modrm >> 3) & 0x7);
  const uint8_t rm = static_cast<uint8_t>(modrm & 0x7);

  if (mod != 0 || rm != 5)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return x64_instruction_outcome::stop_running;
  }

  const uintptr_t next_instruction = instruction.next_address + 5;
  context.get_register64(register_index) = next_instruction + displacement;
  context.get_state().instruction_pointer = next_instruction;
  return x64_instruction_outcome::continue_running;
}

x64_instruction_outcome execute_x64_nop(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return x64_instruction_outcome::stop_running;
  }

  context.get_state().instruction_pointer = instruction.next_address;
  return x64_instruction_outcome::continue_running;
}

