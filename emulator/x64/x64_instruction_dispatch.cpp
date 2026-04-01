#include "x64_instruction_dispatch.h"

#include "x64_execution_context.h"
#include "x64_instruction_handlers.h"

x64_instruction_dispatch::x64_instruction_dispatch()
{
  initialize_opcode_handlers();
}

x64_instruction_outcome x64_instruction_dispatch::dispatch(
  x64_execution_context& context, const x64_decoded_instruction& instruction) const
{
  context.set_fault(instruction);

  if (instruction.opcode == 0x0F)
  {
    return dispatch_secondary_opcode(context, instruction);
  }

  return m_primary_opcode_handlers[instruction.opcode](context, instruction);
}

void x64_instruction_dispatch::initialize_opcode_handlers()
{
  for (uint32_t opcode = 0; opcode < 256; ++opcode)
  {
    m_primary_opcode_handlers[opcode] = &execute_x64_unsupported;
    m_secondary_opcode_handlers[opcode] = &execute_x64_unsupported;
  }

  m_primary_opcode_handlers[0x80] = &execute_x64_group80;
  m_primary_opcode_handlers[0x81] = &execute_x64_group81;
  m_primary_opcode_handlers[0x08] = &execute_x64_or_register_byte;
  m_primary_opcode_handlers[0x0B] = &execute_x64_or_rm_to_register;
  m_primary_opcode_handlers[0x89] = &execute_x64_mov_register;
  m_primary_opcode_handlers[0x8B] = &execute_x64_mov_register;
  m_primary_opcode_handlers[0x39] = &execute_x64_compare_rm_with_register;
  m_primary_opcode_handlers[0x31] = &execute_x64_xor_register;
  m_primary_opcode_handlers[0x85] = &execute_x64_test_register;
  m_primary_opcode_handlers[0xA9] = &execute_x64_test_accumulator_immediate;
  m_primary_opcode_handlers[0x83] = &execute_x64_group83;
  m_primary_opcode_handlers[0x8D] = &execute_x64_lea;
  m_primary_opcode_handlers[0x90] = &execute_x64_nop;
  m_primary_opcode_handlers[0xC7] = &execute_x64_mov_immediate_to_rm;
  m_primary_opcode_handlers[0xF6] = &execute_x64_group_f6;
  m_primary_opcode_handlers[0xF7] = &execute_x64_group_f7;
  m_primary_opcode_handlers[0xC3] = &execute_x64_return;
  m_primary_opcode_handlers[0xE8] = &execute_x64_call_relative;
  m_primary_opcode_handlers[0xE9] = &execute_x64_jump_relative_near;
  m_primary_opcode_handlers[0xEB] = &execute_x64_jump_relative_short;
  m_primary_opcode_handlers[0x74] = &execute_x64_jump_short_condition;
  m_primary_opcode_handlers[0x75] = &execute_x64_jump_short_condition;
  m_primary_opcode_handlers[0xFF] = &execute_x64_call_indirect_memory;

  for (uint32_t opcode = 0x50; opcode <= 0x57; ++opcode)
  {
    m_primary_opcode_handlers[opcode] = &execute_x64_push_register;
  }

  for (uint32_t opcode = 0x58; opcode <= 0x5F; ++opcode)
  {
    m_primary_opcode_handlers[opcode] = &execute_x64_pop_register;
  }

  for (uint32_t opcode = 0xB8; opcode <= 0xBF; ++opcode)
  {
    m_primary_opcode_handlers[opcode] = &execute_x64_mov_immediate32;
  }

  m_secondary_opcode_handlers[0x10] = &execute_x64_secondary_mov_xmm;
  m_secondary_opcode_handlers[0x11] = &execute_x64_secondary_store_xmm;
  m_secondary_opcode_handlers[0x05] = &execute_x64_syscall;
  m_secondary_opcode_handlers[0x29] = &execute_x64_secondary_store_xmm;
  m_secondary_opcode_handlers[0x84] = &execute_x64_jump_near_condition;
  m_secondary_opcode_handlers[0x85] = &execute_x64_jump_near_condition;
  m_secondary_opcode_handlers[0x94] = &execute_x64_secondary_setcc;
  m_secondary_opcode_handlers[0x95] = &execute_x64_secondary_setcc;
  m_secondary_opcode_handlers[0xB6] = &execute_x64_secondary_movzx_byte;
  m_secondary_opcode_handlers[0x57] = &execute_x64_secondary_xorps;
  m_secondary_opcode_handlers[0x60] = &execute_x64_secondary_punpcklbw;
  m_secondary_opcode_handlers[0x6E] = &execute_x64_secondary_movd;
  m_secondary_opcode_handlers[0x70] = &execute_x64_secondary_shuffle;
  m_secondary_opcode_handlers[0x7F] = &execute_x64_secondary_store_xmm;
  m_secondary_opcode_handlers[0xD6] = &execute_x64_secondary_movq_store;
  m_secondary_opcode_handlers[0x1F] = &execute_x64_secondary_nop;
}

x64_instruction_outcome x64_instruction_dispatch::dispatch_secondary_opcode(
  x64_execution_context& context, const x64_decoded_instruction& instruction) const
{
  if (instruction.rex_w)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  uint8_t secondary_opcode = 0;

  if (!context.read_u8(instruction.next_address, &secondary_opcode))
  {
    context.set_invalid_memory_access(instruction.next_address, 0);
    return X64_INSTRUCTION_OUTCOME_STOP_RUNNING;
  }

  context.get_result().fault_opcode = secondary_opcode;
  return m_secondary_opcode_handlers[secondary_opcode](context, instruction);
}
