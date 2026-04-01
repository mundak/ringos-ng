#include "x64_execution_context.h"
#include "x64_instruction_handlers.h"

namespace
{
  bool try_skip_modrm_operand(
    x64_execution_context& context,
    const x64_decoded_instruction& instruction,
    uintptr_t modrm_address,
    uintptr_t* out_next_instruction)
  {
    uint8_t modrm = 0;

    if (out_next_instruction == nullptr || !context.read_u8(modrm_address, &modrm))
    {
      return false;
    }

    const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
    const uint8_t operation = static_cast<uint8_t>((modrm >> 3) & 0x7);
    const uint8_t rm = static_cast<uint8_t>(modrm & 0x7);
    uintptr_t next_instruction = modrm_address + 1;

    if (operation != 0)
    {
      return false;
    }

    if (mod == 3)
    {
      *out_next_instruction = next_instruction;
      return true;
    }

    if (rm == 4)
    {
      uint8_t sib = 0;

      if (!context.read_u8(next_instruction, &sib))
      {
        return false;
      }

      ++next_instruction;

      const uint8_t base = static_cast<uint8_t>(sib & 0x7);

      if (mod == 0 && base == 5 && !instruction.rex_b)
      {
        uint32_t displacement = 0;

        if (!context.read_u32(next_instruction, &displacement))
        {
          return false;
        }

        next_instruction += sizeof(uint32_t);
      }
    }
    else if (mod == 0 && rm == 5 && !instruction.rex_b)
    {
      uint32_t displacement = 0;

      if (!context.read_u32(next_instruction, &displacement))
      {
        return false;
      }

      next_instruction += sizeof(uint32_t);
    }

    if (mod == 1)
    {
      uint8_t displacement = 0;

      if (!context.read_u8(next_instruction, &displacement))
      {
        return false;
      }

      next_instruction += sizeof(uint8_t);
    }
    else if (mod == 2)
    {
      uint32_t displacement = 0;

      if (!context.read_u32(next_instruction, &displacement))
      {
        return false;
      }

      next_instruction += sizeof(uint32_t);
    }

    *out_next_instruction = next_instruction;
    return true;
  }
}

x64_instruction_outcome execute_x64_unsupported(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  context.set_unsupported_instruction(instruction.opcode);
  return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
}

x64_instruction_outcome execute_x64_syscall(x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  bool should_continue = false;
  context.get_state().instruction_pointer = instruction.next_address + 1;

  const int32_t syscall_result
    = context.get_callbacks().handle_syscall(context.get_callbacks().context, context.get_state(), &should_continue);
  context.get_register64(static_cast<uint32_t>(X64_GENERAL_REGISTER_RAX))
    = static_cast<uint64_t>(static_cast<int64_t>(syscall_result));

  if (!should_continue)
  {
    context.get_result().completion = X64_EMULATOR_COMPLETION_THREAD_EXITED;
    return X64_INSTRUCTION_OUTCOME_RETIRE_AND_STOP;
  }

  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}

x64_instruction_outcome execute_x64_secondary_nop(
  x64_execution_context& context, const x64_decoded_instruction& instruction)
{
  uintptr_t next_instruction = 0;

  if (!try_skip_modrm_operand(context, instruction, instruction.next_address + 1, &next_instruction))
  {
    context.set_unsupported_instruction(0x1F);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.get_state().instruction_pointer = next_instruction;
  return X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING;
}
