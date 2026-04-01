#include "x64_execution_context.h"
#include "x64_instruction_handlers.h"

namespace
{
  struct x64_decoded_rm_operand
  {
    uint8_t mod;
    uint8_t operation;
    uint32_t rm_index;
    bool rm_is_register;
    uintptr_t rm_address;
    uintptr_t next_instruction;
  };

  bool try_read_sib_operand_address(
    x64_execution_context& context,
    const x64_decoded_instruction& instruction,
    uintptr_t modrm_address,
    uint8_t mod,
    uintptr_t* out_address,
    uintptr_t* out_next_instruction)
  {
    uint8_t sib = 0;

    if (out_address == nullptr || out_next_instruction == nullptr || !context.read_u8(modrm_address + 1, &sib))
    {
      return false;
    }

    const uint8_t scale_shift = static_cast<uint8_t>((sib >> 6) & 0x3);
    const uint8_t index_field = static_cast<uint8_t>((sib >> 3) & 0x7);
    const uint8_t base_field = static_cast<uint8_t>(sib & 0x7);
    const uintptr_t displacement_address = modrm_address + 2;
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

  bool try_decode_rm_operand(
    x64_execution_context& context,
    const x64_decoded_instruction& instruction,
    uintptr_t modrm_address,
    x64_decoded_rm_operand* out_operand)
  {
    uint8_t modrm = 0;

    if (out_operand == nullptr || !context.read_u8(modrm_address, &modrm))
    {
      return false;
    }

    const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
    const uint8_t rm = static_cast<uint8_t>(modrm & 0x7);
    const uint32_t rm_index = static_cast<uint32_t>(rm + (instruction.rex_b ? 8U : 0U));

    out_operand->mod = mod;
    out_operand->operation = static_cast<uint8_t>((modrm >> 3) & 0x7);
    out_operand->rm_index = rm_index;
    out_operand->rm_is_register = mod == 3;
    out_operand->rm_address = 0;
    out_operand->next_instruction = modrm_address + 1;

    if (mod == 3)
    {
      return true;
    }

    if (rm == 4)
    {
      return try_read_sib_operand_address(
        context, instruction, modrm_address, mod, &out_operand->rm_address, &out_operand->next_instruction);
    }

    if (mod == 0)
    {
      if (rm == 5 && !instruction.rex_b)
      {
        int32_t displacement = 0;

        if (!context.read_i32(modrm_address + 1, &displacement))
        {
          return false;
        }

        out_operand->next_instruction = modrm_address + 5;
        out_operand->rm_address = out_operand->next_instruction + displacement;
        return true;
      }

      out_operand->rm_address = static_cast<uintptr_t>(context.get_register64(rm_index));
      return true;
    }

    if (mod == 1)
    {
      int8_t displacement = 0;

      if (!context.read_i8(modrm_address + 1, &displacement))
      {
        return false;
      }

      out_operand->next_instruction = modrm_address + 2;
      out_operand->rm_address
        = static_cast<uintptr_t>(static_cast<int64_t>(context.get_register64(rm_index)) + displacement);
      return true;
    }

    if (mod == 2)
    {
      int32_t displacement = 0;

      if (!context.read_i32(modrm_address + 1, &displacement))
      {
        return false;
      }

      out_operand->next_instruction = modrm_address + 5;
      out_operand->rm_address
        = static_cast<uintptr_t>(static_cast<int64_t>(context.get_register64(rm_index)) + displacement);
      return true;
    }

    return false;
  }
}

x64_instruction_outcome execute_x64_xor_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (instruction.rex_w)
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
  const uint32_t source_register = static_cast<uint32_t>(((modrm >> 3) & 0x7) + (instruction.rex_r ? 8U : 0U));
  const uint32_t destination_register = static_cast<uint32_t>((modrm & 0x7) + (instruction.rex_b ? 8U : 0U));

  if (mod != 3)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint32_t result_value = context.get_register32(destination_register) ^ context.get_register32(source_register);
  context.set_register32(destination_register, result_value);
  context.set_logic_flags(result_value, false);
  context.get_state().instruction_pointer = instruction.next_address + 1;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_or_register_byte(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uint8_t modrm = 0;

  if (!context.read_u8(instruction.next_address, &modrm))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint32_t source_register = static_cast<uint32_t>(((modrm >> 3) & 0x7) + (instruction.rex_r ? 8U : 0U));
  const uint32_t destination_register = static_cast<uint32_t>((modrm & 0x7) + (instruction.rex_b ? 8U : 0U));

  if (mod != 3)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint8_t result_value = static_cast<uint8_t>(
    context.get_register8_low(destination_register) | context.get_register8_low(source_register));
  context.set_register8_low(destination_register, result_value);
  context.set_logic_flags(result_value, false);
  context.get_state().instruction_pointer = instruction.next_address + 1;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_or_rm_to_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  x64_decoded_rm_operand operand {};
  uint8_t modrm = 0;

  if (
    !context.read_u8(instruction.next_address, &modrm)
    || !try_decode_rm_operand(context, instruction, instruction.next_address, &operand))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint32_t destination_register = static_cast<uint32_t>(((modrm >> 3) & 0x7) + (instruction.rex_r ? 8U : 0U));

  if (instruction.rex_w)
  {
    uint64_t rhs = 0;

    if (operand.rm_is_register)
    {
      rhs = context.get_register64(operand.rm_index);
    }
    else if (!context.read_u64(operand.rm_address, &rhs))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    const uint64_t result_value = context.get_register64(destination_register) | rhs;
    context.get_register64(destination_register) = result_value;
    context.set_logic_flags(result_value, true);
  }
  else
  {
    uint32_t rhs = 0;

    if (operand.rm_is_register)
    {
      rhs = context.get_register32(operand.rm_index);
    }
    else if (!context.read_u32(operand.rm_address, &rhs))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    const uint32_t result_value = context.get_register32(destination_register) | rhs;
    context.set_register32(destination_register, result_value);
    context.set_logic_flags(result_value, false);
  }

  context.get_state().instruction_pointer = operand.next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_test_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uint8_t modrm = 0;

  if (!context.read_u8(instruction.next_address, &modrm))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
  const uint32_t source_register = static_cast<uint32_t>(((modrm >> 3) & 0x7) + (instruction.rex_r ? 8U : 0U));
  const uint32_t destination_register = static_cast<uint32_t>((modrm & 0x7) + (instruction.rex_b ? 8U : 0U));

  if (mod != 3)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
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
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_test_accumulator_immediate(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uint32_t immediate = 0;

  if (!context.read_u32(instruction.next_address, &immediate))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  if (instruction.rex_w)
  {
    const uint64_t value = context.get_register64(static_cast<uint32_t>(X64_GENERAL_REGISTER_RAX));
    const uint64_t mask = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(immediate)));
    context.set_logic_flags(value & mask, true);
  }
  else
  {
    const uint32_t value = context.get_register32(static_cast<uint32_t>(X64_GENERAL_REGISTER_RAX));
    context.set_logic_flags(value & immediate, false);
  }

  context.get_state().instruction_pointer = instruction.next_address + sizeof(uint32_t);
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_group_f6(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  x64_decoded_rm_operand operand {};
  uint8_t immediate = 0;

  if (
    !try_decode_rm_operand(context, instruction, instruction.next_address, &operand)
    || !context.read_u8(operand.next_instruction, &immediate))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  if (operand.operation != 0)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  uint8_t lhs = 0;

  if (operand.rm_is_register)
  {
    lhs = context.get_register8_low(operand.rm_index);
  }
  else if (!context.read_u8(operand.rm_address, &lhs))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.set_logic_flags(static_cast<uint8_t>(lhs & immediate), false);
  context.get_state().instruction_pointer = operand.next_instruction + sizeof(uint8_t);
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_group_f7(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  x64_decoded_rm_operand operand {};

  if (!try_decode_rm_operand(context, instruction, instruction.next_address, &operand))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  if (operand.operation != 3)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  if (instruction.rex_w)
  {
    uint64_t rhs = 0;

    if (operand.rm_is_register)
    {
      rhs = context.get_register64(operand.rm_index);
    }
    else if (!context.read_u64(operand.rm_address, &rhs))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    const uint64_t result_value = static_cast<uint64_t>(0) - rhs;

    if (operand.rm_is_register)
    {
      context.get_register64(operand.rm_index) = result_value;
    }
    else if (!context.write_u64(operand.rm_address, result_value))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    context.set_compare_flags(0, rhs, result_value, true);
  }
  else
  {
    uint32_t rhs = 0;

    if (operand.rm_is_register)
    {
      rhs = context.get_register32(operand.rm_index);
    }
    else if (!context.read_u32(operand.rm_address, &rhs))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    const uint32_t result_value = static_cast<uint32_t>(0) - rhs;

    if (operand.rm_is_register)
    {
      context.set_register32(operand.rm_index, result_value);
    }
    else if (!context.write_u32(operand.rm_address, result_value))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    context.set_compare_flags(0, rhs, result_value, false);
  }

  context.get_state().instruction_pointer = operand.next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_group83(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  x64_decoded_rm_operand operand {};
  int8_t immediate = 0;

  if (
    !try_decode_rm_operand(context, instruction, instruction.next_address, &operand)
    || !context.read_i8(operand.next_instruction, &immediate))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  if (instruction.rex_w)
  {
    uint64_t lhs = 0;
    const uint64_t rhs = static_cast<uint64_t>(static_cast<int64_t>(immediate));

    if (operand.rm_is_register)
    {
      lhs = context.get_register64(operand.rm_index);
    }
    else if (!context.read_u64(operand.rm_address, &lhs))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    if (operand.operation == 0)
    {
      const uint64_t sum = lhs + rhs;

      if (operand.rm_is_register)
      {
        context.get_register64(operand.rm_index) = sum;
      }
      else if (!context.write_u64(operand.rm_address, sum))
      {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
      }

      context.set_add_flags(lhs, rhs, sum, true);
    }
    else if (operand.operation == 5)
    {
      const uint64_t difference = lhs - rhs;

      if (operand.rm_is_register)
      {
        context.get_register64(operand.rm_index) = difference;
      }
      else if (!context.write_u64(operand.rm_address, difference))
      {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
      }

      context.set_compare_flags(lhs, rhs, difference, true);
    }
    else if (operand.operation == 7)
    {
      const uint64_t difference = lhs - rhs;
      context.set_compare_flags(lhs, rhs, difference, true);
    }
    else
    {
      context.set_unsupported_instruction(instruction.opcode);
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }
  }
  else
  {
    uint32_t lhs = 0;
    const uint32_t rhs = static_cast<uint32_t>(static_cast<int32_t>(immediate));

    if (operand.rm_is_register)
    {
      lhs = context.get_register32(operand.rm_index);
    }
    else if (!context.read_u32(operand.rm_address, &lhs))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    if (operand.operation == 0)
    {
      const uint32_t sum = lhs + rhs;

      if (operand.rm_is_register)
      {
        context.set_register32(operand.rm_index, sum);
      }
      else if (!context.write_u32(operand.rm_address, sum))
      {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
      }

      context.set_add_flags(lhs, rhs, sum, false);
    }
    else if (operand.operation == 5)
    {
      const uint32_t difference = lhs - rhs;

      if (operand.rm_is_register)
      {
        context.set_register32(operand.rm_index, difference);
      }
      else if (!context.write_u32(operand.rm_address, difference))
      {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
      }

      context.set_compare_flags(lhs, rhs, difference, false);
    }
    else if (operand.operation == 7)
    {
      const uint32_t difference = lhs - rhs;
      context.set_compare_flags(lhs, rhs, difference, false);
    }
    else
    {
      context.set_unsupported_instruction(instruction.opcode);
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }
  }

  context.get_state().instruction_pointer = operand.next_instruction + 1;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_compare_rm_with_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  x64_decoded_rm_operand operand {};
  uint8_t modrm = 0;

  if (
    !context.read_u8(instruction.next_address, &modrm)
    || !try_decode_rm_operand(context, instruction, instruction.next_address, &operand))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  const uint32_t source_register = static_cast<uint32_t>(((modrm >> 3) & 0x7) + (instruction.rex_r ? 8U : 0U));

  if (instruction.rex_w)
  {
    uint64_t lhs = 0;
    const uint64_t rhs = context.get_register64(source_register);

    if (operand.rm_is_register)
    {
      lhs = context.get_register64(operand.rm_index);
    }
    else if (!context.read_u64(operand.rm_address, &lhs))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    context.set_compare_flags(lhs, rhs, lhs - rhs, true);
  }
  else
  {
    uint32_t lhs = 0;
    const uint32_t rhs = context.get_register32(source_register);

    if (operand.rm_is_register)
    {
      lhs = context.get_register32(operand.rm_index);
    }
    else if (!context.read_u32(operand.rm_address, &lhs))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    context.set_compare_flags(lhs, rhs, lhs - rhs, false);
  }

  context.get_state().instruction_pointer = operand.next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_group81(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  x64_decoded_rm_operand operand {};
  int32_t immediate = 0;

  if (
    !try_decode_rm_operand(context, instruction, instruction.next_address, &operand)
    || !context.read_i32(operand.next_instruction, &immediate))
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  if (instruction.rex_w)
  {
    uint64_t lhs = 0;
    const uint64_t rhs = static_cast<uint64_t>(static_cast<int64_t>(immediate));

    if (operand.rm_is_register)
    {
      lhs = context.get_register64(operand.rm_index);
    }
    else if (!context.read_u64(operand.rm_address, &lhs))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    if (operand.operation == 0)
    {
      const uint64_t sum = lhs + rhs;

      if (operand.rm_is_register)
      {
        context.get_register64(operand.rm_index) = sum;
      }
      else if (!context.write_u64(operand.rm_address, sum))
      {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
      }

      context.set_add_flags(lhs, rhs, sum, true);
    }
    else if (operand.operation == 5)
    {
      const uint64_t difference = lhs - rhs;

      if (operand.rm_is_register)
      {
        context.get_register64(operand.rm_index) = difference;
      }
      else if (!context.write_u64(operand.rm_address, difference))
      {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
      }

      context.set_compare_flags(lhs, rhs, difference, true);
    }
    else if (operand.operation == 7)
    {
      const uint64_t difference = lhs - rhs;
      context.set_compare_flags(lhs, rhs, difference, true);
    }
    else
    {
      context.set_unsupported_instruction(instruction.opcode);
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }
  }
  else
  {
    uint32_t lhs = 0;
    const uint32_t rhs = static_cast<uint32_t>(immediate);

    if (operand.rm_is_register)
    {
      lhs = context.get_register32(operand.rm_index);
    }
    else if (!context.read_u32(operand.rm_address, &lhs))
    {
      context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }

    if (operand.operation == 0)
    {
      const uint32_t sum = lhs + rhs;

      if (operand.rm_is_register)
      {
        context.set_register32(operand.rm_index, sum);
      }
      else if (!context.write_u32(operand.rm_address, sum))
      {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
      }

      context.set_add_flags(lhs, rhs, sum, false);
    }
    else if (operand.operation == 5)
    {
      const uint32_t difference = lhs - rhs;

      if (operand.rm_is_register)
      {
        context.set_register32(operand.rm_index, difference);
      }
      else if (!context.write_u32(operand.rm_address, difference))
      {
        context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
        return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
      }

      context.set_compare_flags(lhs, rhs, difference, false);
    }
    else if (operand.operation == 7)
    {
      const uint32_t difference = lhs - rhs;
      context.set_compare_flags(lhs, rhs, difference, false);
    }
    else
    {
      context.set_unsupported_instruction(instruction.opcode);
      return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
    }
  }

  context.get_state().instruction_pointer = operand.next_instruction + sizeof(int32_t);
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}
