#include "x64_execution_context.h"
#include "x64_instruction_handlers.h"
#include "klibc/memory.h"

namespace
{
  struct x64_decoded_modrm_operand
  {
    uint8_t mod;
    uint32_t reg_index;
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

  bool try_decode_modrm_operand(
    x64_execution_context& context,
    const x64_decoded_instruction& instruction,
    uintptr_t modrm_address,
    x64_decoded_modrm_operand* out_operand)
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
    out_operand->reg_index = static_cast<uint32_t>(((modrm >> 3) & 0x7) + (instruction.rex_r ? 8U : 0U));
    out_operand->rm_index = rm_index;
    out_operand->rm_is_register = mod == 3;
    out_operand->next_instruction = modrm_address + 1;
    out_operand->rm_address = 0;

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

  bool try_read_simd_operand(
    x64_execution_context& context, const x64_decoded_modrm_operand& operand, x64_simd_register* out_value)
  {
    if (out_value == nullptr)
    {
      return false;
    }

    if (operand.rm_is_register)
    {
      *out_value = context.get_simd_register(operand.rm_index);
      return true;
    }

    return context.read_u128(operand.rm_address, out_value);
  }

  bool try_read_rm32_operand(
    x64_execution_context& context, const x64_decoded_modrm_operand& operand, uint32_t* out_value)
  {
    if (out_value == nullptr)
    {
      return false;
    }

    if (operand.rm_is_register)
    {
      *out_value = context.get_register32(operand.rm_index);
      return true;
    }

    return context.read_u32(operand.rm_address, out_value);
  }

  void zero_simd_register(x64_simd_register* value)
  {
    if (value == nullptr)
    {
      return;
    }

    for (uint32_t index = 0; index < 16; ++index)
    {
      value->bytes[index] = 0;
    }
  }

  x64_instruction_outcome fail_invalid_memory(x64_execution_context& context)
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  bool try_read_secondary_opcode(
    x64_execution_context& context, const x64_decoded_instruction& instruction, uint8_t* out_secondary_opcode)
  {
    return out_secondary_opcode != nullptr && context.read_u8(instruction.next_address, out_secondary_opcode);
  }
}

x64_instruction_outcome execute_x64_secondary_mov_xmm(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (instruction.prefix_66 || instruction.prefix_f2 || instruction.prefix_f3)
  {
    context.set_unsupported_instruction(0x10);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  x64_decoded_modrm_operand operand {};

  if (!try_decode_modrm_operand(context, instruction, instruction.next_address + 1, &operand))
  {
    return fail_invalid_memory(context);
  }

  x64_simd_register source {};

  if (!try_read_simd_operand(context, operand, &source))
  {
    return fail_invalid_memory(context);
  }

  context.get_simd_register(operand.reg_index) = source;
  context.get_state().instruction_pointer = operand.next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_secondary_store_xmm(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uint8_t secondary_opcode = 0;

  if (!try_read_secondary_opcode(context, instruction, &secondary_opcode))
  {
    return fail_invalid_memory(context);
  }

  const bool is_movups
    = secondary_opcode == 0x11 && !instruction.prefix_66 && !instruction.prefix_f2 && !instruction.prefix_f3;
  const bool is_movaps
    = secondary_opcode == 0x29 && !instruction.prefix_66 && !instruction.prefix_f2 && !instruction.prefix_f3;
  const bool is_movdqu
    = secondary_opcode == 0x7F && instruction.prefix_f3 && !instruction.prefix_66 && !instruction.prefix_f2;

  if (!is_movups && !is_movaps && !is_movdqu)
  {
    context.set_unsupported_instruction(secondary_opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  x64_decoded_modrm_operand operand {};

  if (!try_decode_modrm_operand(context, instruction, instruction.next_address + 1, &operand))
  {
    return fail_invalid_memory(context);
  }

  if (operand.rm_is_register)
  {
    context.get_simd_register(operand.rm_index) = context.get_simd_register(operand.reg_index);
  }
  else if (!context.write_u128(operand.rm_address, context.get_simd_register(operand.reg_index)))
  {
    return fail_invalid_memory(context);
  }

  context.get_state().instruction_pointer = operand.next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_secondary_xorps(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (instruction.prefix_66 || instruction.prefix_f2 || instruction.prefix_f3)
  {
    context.set_unsupported_instruction(0x57);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  x64_decoded_modrm_operand operand {};

  if (!try_decode_modrm_operand(context, instruction, instruction.next_address + 1, &operand))
  {
    return fail_invalid_memory(context);
  }

  x64_simd_register source {};

  if (!try_read_simd_operand(context, operand, &source))
  {
    return fail_invalid_memory(context);
  }

  x64_simd_register& destination = context.get_simd_register(operand.reg_index);

  for (uint32_t index = 0; index < 16; ++index)
  {
    destination.bytes[index] ^= source.bytes[index];
  }

  context.get_state().instruction_pointer = operand.next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_secondary_movd(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (!instruction.prefix_66 || instruction.prefix_f2 || instruction.prefix_f3)
  {
    context.set_unsupported_instruction(0x6E);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  x64_decoded_modrm_operand operand {};

  if (!try_decode_modrm_operand(context, instruction, instruction.next_address + 1, &operand))
  {
    return fail_invalid_memory(context);
  }

  uint32_t source = 0;

  if (!try_read_rm32_operand(context, operand, &source))
  {
    return fail_invalid_memory(context);
  }

  x64_simd_register& destination = context.get_simd_register(operand.reg_index);
  zero_simd_register(&destination);
  memcpy(destination.bytes, &source, sizeof(source));
  context.get_state().instruction_pointer = operand.next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_secondary_punpcklbw(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (!instruction.prefix_66 || instruction.prefix_f2 || instruction.prefix_f3)
  {
    context.set_unsupported_instruction(0x60);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  x64_decoded_modrm_operand operand {};

  if (!try_decode_modrm_operand(context, instruction, instruction.next_address + 1, &operand))
  {
    return fail_invalid_memory(context);
  }

  x64_simd_register source {};

  if (!try_read_simd_operand(context, operand, &source))
  {
    return fail_invalid_memory(context);
  }

  x64_simd_register original_destination = context.get_simd_register(operand.reg_index);
  x64_simd_register result {};

  for (uint32_t index = 0; index < 8; ++index)
  {
    result.bytes[index * 2] = original_destination.bytes[index];
    result.bytes[index * 2 + 1] = source.bytes[index];
  }

  context.get_simd_register(operand.reg_index) = result;
  context.get_state().instruction_pointer = operand.next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_secondary_shuffle(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  const bool is_pshufd = instruction.prefix_66 && !instruction.prefix_f2 && !instruction.prefix_f3;
  const bool is_pshuflw = instruction.prefix_f2 && !instruction.prefix_66 && !instruction.prefix_f3;

  if (!is_pshufd && !is_pshuflw)
  {
    context.set_unsupported_instruction(0x70);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  x64_decoded_modrm_operand operand {};
  uint8_t immediate = 0;

  if (!try_decode_modrm_operand(context, instruction, instruction.next_address + 1, &operand))
  {
    return fail_invalid_memory(context);
  }

  if (!context.read_u8(operand.next_instruction, &immediate))
  {
    return fail_invalid_memory(context);
  }

  x64_simd_register source {};

  if (!try_read_simd_operand(context, operand, &source))
  {
    return fail_invalid_memory(context);
  }

  x64_simd_register result = source;

  if (is_pshufd)
  {
    for (uint32_t index = 0; index < 4; ++index)
    {
      const uint32_t source_index = (immediate >> (index * 2)) & 0x3;

      memcpy(result.bytes + index * 4, source.bytes + source_index * 4, 4);
    }
  }
  else
  {
    for (uint32_t index = 0; index < 4; ++index)
    {
      const uint32_t source_index = (immediate >> (index * 2)) & 0x3;

      memcpy(result.bytes + index * 2, source.bytes + source_index * 2, 2);
    }
  }

  context.get_simd_register(operand.reg_index) = result;
  context.get_state().instruction_pointer = operand.next_instruction + 1;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_secondary_movq_store(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  if (!instruction.prefix_66 || instruction.prefix_f2 || instruction.prefix_f3)
  {
    context.set_unsupported_instruction(0xD6);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  x64_decoded_modrm_operand operand {};

  if (!try_decode_modrm_operand(context, instruction, instruction.next_address + 1, &operand))
  {
    return fail_invalid_memory(context);
  }

  if (operand.rm_is_register)
  {
    context.set_unsupported_instruction(0xD6);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  uint64_t low_qword = 0;
  memcpy(&low_qword, context.get_simd_register(operand.reg_index).bytes, sizeof(low_qword));

  if (!context.write_u64(operand.rm_address, low_qword))
  {
    return fail_invalid_memory(context);
  }

  context.get_state().instruction_pointer = operand.next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}
