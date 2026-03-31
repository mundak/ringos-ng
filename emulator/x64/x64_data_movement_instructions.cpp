#include "x64_execution_context.h"
#include "x64_instruction_handlers.h"

namespace
{
  bool try_get_memory_operand_address(
    x64_execution_context& context,
    const x64_decoded_instruction& instruction,
    uint8_t modrm,
    uintptr_t* out_address,
    uintptr_t* out_next_instruction)
  {
    if (out_address == nullptr || out_next_instruction == nullptr)
    {
      return false;
    }

    const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
    const uint8_t rm = static_cast<uint8_t>(modrm & 0x7);
    const uint32_t base_register_index = rm + (instruction.rex_b ? 8U : 0U);

    if (mod == 0)
    {
      if (rm == 4)
      {
        return false;
      }

      if (rm == 5)
      {
        int32_t displacement = 0;

        if (!context.read_i32(instruction.next_address + 1, &displacement))
        {
          context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
          return false;
        }

        *out_next_instruction = instruction.next_address + 5;
        *out_address = *out_next_instruction + displacement;
        return true;
      }

      *out_next_instruction = instruction.next_address + 1;
      *out_address = static_cast<uintptr_t>(context.get_register64(base_register_index));
      return true;
    }

    if (mod == 1)
    {
      if (rm == 4)
      {
        return false;
      }

      int8_t displacement = 0;

            if (!context.read_i8(instruction.next_address + 1, &displacement))
            {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return false;
            }

      *out_next_instruction = instruction.next_address + 2;
      *out_address
        = static_cast<uintptr_t>(static_cast<int64_t>(context.get_register64(base_register_index)) + displacement);
      return true;
    }

    if (mod == 2)
    {
      if (rm == 4)
      {
        return false;
      }

      int32_t displacement = 0;

            if (!context.read_i32(instruction.next_address + 1, &displacement))
            {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return false;
            }

      *out_next_instruction = instruction.next_address + 5;
      *out_address
        = static_cast<uintptr_t>(static_cast<int64_t>(context.get_register64(base_register_index)) + displacement);
      return true;
    }

    return false;
  }

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
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint32_t register_index = (instruction.opcode - 0xB8) + (instruction.rex_b ? 8U : 0U);
  uint32_t immediate = 0;

  if (!context.read_u32(instruction.next_address, &immediate))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.set_register32(register_index, immediate);
  context.get_state().instruction_pointer = instruction.next_address + sizeof(uint32_t);
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_mov_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uint8_t modrm = 0;

  if (!context.read_u8(instruction.next_address, &modrm))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint32_t register_index = static_cast<uint32_t>(((modrm >> 3) & 0x7) + (instruction.rex_r ? 8U : 0U));
  const uint32_t rm_register_index = static_cast<uint32_t>((modrm & 0x7) + (instruction.rex_b ? 8U : 0U));

  if (mod == 3)
  {
    if (instruction.opcode == 0x89)
    {
      if (instruction.rex_w)
      {
        context.get_register64(rm_register_index) = context.get_register64(register_index);
      }
      else
      {
        context.set_register32(rm_register_index, context.get_register32(register_index));
      }
    }
    else if (instruction.opcode == 0x8B)
    {
      if (instruction.rex_w)
      {
        context.get_register64(register_index) = context.get_register64(rm_register_index);
      }
      else
      {
        context.set_register32(register_index, context.get_register32(rm_register_index));
      }
    }
    else
    {
      context.set_unsupported_instruction(instruction.opcode);
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    context.get_state().instruction_pointer = instruction.next_address + 1;
    return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
  }

  uintptr_t memory_address = 0;
  uintptr_t next_instruction = 0;

  if (!try_get_memory_operand_address(context, instruction, modrm, &memory_address, &next_instruction))
  {
    if (context.get_result().completion == X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS)
    {
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  if (instruction.rex_w)
  {
    if (instruction.opcode == 0x89)
    {
            if (!context.write_u64(memory_address, context.get_register64(register_index)))
            {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
            }
    }
    else if (instruction.opcode == 0x8B)
    {
      uint64_t value = 0;

            if (!context.read_u64(memory_address, &value))
            {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
            }

      context.get_register64(register_index) = value;
    }
    else
    {
      context.set_unsupported_instruction(instruction.opcode);
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }
  }
  else
  {
    if (instruction.opcode == 0x89)
    {
            if (!context.write_u32(memory_address, context.get_register32(register_index)))
            {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
            }
    }
    else if (instruction.opcode == 0x8B)
    {
      uint32_t value = 0;

            if (!context.read_u32(memory_address, &value))
            {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
            }

      context.set_register32(register_index, value);
    }
    else
    {
      context.set_unsupported_instruction(instruction.opcode);
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }
  }

  context.get_state().instruction_pointer = next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_lea_rip_relative(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (!instruction.rex_w)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  uint8_t modrm = 0;
  int32_t displacement = 0;

  if (
    !context.read_u8(instruction.next_address, &modrm)
    || !context.read_i32(instruction.next_address + 1, &displacement))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint8_t register_index = static_cast<uint8_t>((modrm >> 3) & 0x7);
  const uint8_t rm = static_cast<uint8_t>(modrm & 0x7);

  if (mod != 0 || rm != 5)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uintptr_t next_instruction = instruction.next_address + 5;
  context.get_register64(register_index) = next_instruction + displacement;
  context.get_state().instruction_pointer = next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_nop(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (reject_rex_w(context, instruction))
  {
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.get_state().instruction_pointer = instruction.next_address;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

