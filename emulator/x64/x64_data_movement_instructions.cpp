#include "x64_execution_context.h"
#include "x64_instruction_handlers.h"

namespace
{
  bool try_read_sib_address(
    x64_execution_context& context,
    const x64_decoded_instruction& instruction,
    uint8_t mod,
    uintptr_t* out_address,
    uintptr_t* out_next_instruction)
  {
    uint8_t sib = 0;

    if (!context.read_u8(instruction.next_address + 1, &sib))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return false;
    }

    const uint8_t scale_shift = static_cast<uint8_t>((sib >> 6) & 0x3);
    const uint8_t index_field = static_cast<uint8_t>((sib >> 3) & 0x7);
    const uint8_t base_field = static_cast<uint8_t>(sib & 0x7);
    const uintptr_t displacement_address = instruction.next_address + 2;
    const uint64_t scale = 1ULL << scale_shift;
    uint64_t base_value = 0;
    uint64_t index_value = 0;
    int64_t displacement = 0;

    if (index_field != 4 || instruction.rex_x)
    {
      index_value = context.get_register64(index_field + (instruction.rex_x ? 8U : 0U));
    }

    if (mod == 0)
    {
      if (base_field == 5 && !instruction.rex_b)
      {
        int32_t displacement32 = 0;

        if (!context.read_i32(displacement_address, &displacement32))
        {
          context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
          return false;
        }

        *out_next_instruction = displacement_address + sizeof(int32_t);
        *out_address = static_cast<uintptr_t>(index_value * scale + static_cast<int64_t>(displacement32));
        return true;
      }

      base_value = context.get_register64(base_field + (instruction.rex_b ? 8U : 0U));
      *out_next_instruction = displacement_address;
      *out_address = static_cast<uintptr_t>(base_value + index_value * scale);
      return true;
    }

    if (mod == 1)
    {
      int8_t displacement8 = 0;

      if (!context.read_i8(displacement_address, &displacement8))
      {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return false;
      }

      displacement = displacement8;
      *out_next_instruction = displacement_address + sizeof(int8_t);
    }
    else if (mod == 2)
    {
      int32_t displacement32 = 0;

      if (!context.read_i32(displacement_address, &displacement32))
      {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return false;
      }

      displacement = displacement32;
      *out_next_instruction = displacement_address + sizeof(int32_t);
    }
    else
    {
      return false;
    }

    base_value = context.get_register64(base_field + (instruction.rex_b ? 8U : 0U));
    *out_address = static_cast<uintptr_t>(base_value + index_value * scale + displacement);
    return true;
  }

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
        return try_read_sib_address(context, instruction, mod, out_address, out_next_instruction);
      }

      if (rm == 5 && !instruction.rex_b)
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
        return try_read_sib_address(context, instruction, mod, out_address, out_next_instruction);
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
        return try_read_sib_address(context, instruction, mod, out_address, out_next_instruction);
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

x64_instruction_outcome execute_x64_mov_immediate_to_rm(
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
  const uint32_t rm_register_index = static_cast<uint32_t>((modrm & 0x7) + (instruction.rex_b ? 8U : 0U));

  if (operation != 0)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  if (mod == 3)
  {
    uint32_t immediate = 0;

    if (!context.read_u32(instruction.next_address + 1, &immediate))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    if (instruction.rex_w)
    {
      context.get_register64(rm_register_index)
        = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(immediate)));
    }
    else
    {
      context.set_register32(rm_register_index, immediate);
    }

    context.get_state().instruction_pointer = instruction.next_address + 1 + sizeof(uint32_t);
    return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
  }

  uintptr_t memory_address = 0;
  uintptr_t next_instruction = 0;
  uint32_t immediate = 0;

  if (!try_get_memory_operand_address(context, instruction, modrm, &memory_address, &next_instruction))
  {
    if (context.get_result().completion == X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS)
    {
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  if (!context.read_u32(next_instruction, &immediate))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  if (instruction.rex_w)
  {
    const uint64_t immediate64 = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(immediate)));

    if (!context.write_u64(memory_address, immediate64))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }
  }
  else if (!context.write_u32(memory_address, immediate))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.get_state().instruction_pointer = next_instruction + sizeof(uint32_t);
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

x64_instruction_outcome execute_x64_lea(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (!instruction.rex_w)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  uint8_t modrm = 0;

  if (!context.read_u8(instruction.next_address, &modrm))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint8_t register_index = static_cast<uint8_t>(((modrm >> 3) & 0x7) + (instruction.rex_r ? 8U : 0U));

  if (mod == 3)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
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

  context.get_register64(register_index) = memory_address;
  context.get_state().instruction_pointer = next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_group80(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uint8_t modrm = 0;

  if (!context.read_u8(instruction.next_address, &modrm))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint8_t operation = static_cast<uint8_t>((modrm >> 3) & 0x7);
  const uint32_t register_index = static_cast<uint32_t>((modrm & 0x7) + (instruction.rex_b ? 8U : 0U));
  uint8_t lhs = 0;
  uintptr_t next_instruction = instruction.next_address + 1;
  uint8_t immediate = 0;

  if (operation != 7)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  if (mod == 3)
  {
    lhs = context.get_register8_low(register_index);
    next_instruction = instruction.next_address + 1;
  }
  else
  {
    uintptr_t memory_address = 0;

    if (!try_get_memory_operand_address(context, instruction, modrm, &memory_address, &next_instruction))
    {
      if (context.get_result().completion == X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS)
      {
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
      }

      context.set_unsupported_instruction(instruction.opcode);
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    if (!context.read_u8(memory_address, &lhs))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }
  }

  if (!context.read_u8(next_instruction, &immediate))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint8_t result_value = static_cast<uint8_t>(lhs - immediate);
  context.set_compare_flags(lhs, immediate, result_value, false);
  context.get_state().instruction_pointer = next_instruction + sizeof(uint8_t);
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_secondary_setcc(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uint8_t secondary_opcode = 0;
  uint8_t modrm = 0;

  if (
    !context.read_u8(instruction.next_address, &secondary_opcode)
    || !context.read_u8(instruction.next_address + 1, &modrm))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint32_t destination_register = static_cast<uint32_t>((modrm & 0x7) + (instruction.rex_b ? 8U : 0U));

  if (mod != 3 || (secondary_opcode != 0x94 && secondary_opcode != 0x95))
  {
    context.set_unsupported_instruction(secondary_opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const bool zero_flag_set = (context.get_state().flags & (1ULL << 6)) != 0;
  const bool set_condition = secondary_opcode == 0x94 ? zero_flag_set : !zero_flag_set;
  context.set_register8_low(destination_register, static_cast<uint8_t>(set_condition ? 1 : 0));
  context.get_state().instruction_pointer = instruction.next_address + 2;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_secondary_movzx_byte(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uint8_t modrm = 0;

  if (!context.read_u8(instruction.next_address + 1, &modrm))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint32_t destination_register = static_cast<uint32_t>(((modrm >> 3) & 0x7) + (instruction.rex_r ? 8U : 0U));
  const uint32_t source_register = static_cast<uint32_t>((modrm & 0x7) + (instruction.rex_b ? 8U : 0U));
  uintptr_t next_instruction = instruction.next_address + 2;
  uint8_t value = 0;

  if (mod == 3)
  {
    value = context.get_register8_low(source_register);
  }
  else
  {
    uintptr_t memory_address = 0;

    if (!try_get_memory_operand_address(context, instruction, modrm, &memory_address, &next_instruction))
    {
      if (context.get_result().completion == X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS)
      {
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
      }

      context.set_unsupported_instruction(0xB6);
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    if (!context.read_u8(memory_address, &value))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }
  }

  context.set_register32(destination_register, value);
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
