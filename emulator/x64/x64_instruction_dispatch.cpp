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

  m_primary_opcode_handlers[0x31] = &execute_x64_xor_register;
  m_primary_opcode_handlers[0x83] = &execute_x64_group83;
  m_primary_opcode_handlers[0x8D] = &execute_x64_lea_rip_relative;
  m_primary_opcode_handlers[0x90] = &execute_x64_nop;
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

  m_secondary_opcode_handlers[0x05] = &execute_x64_syscall;
}

x64_instruction_outcome x64_instruction_dispatch::dispatch_secondary_opcode(
  x64_execution_context& context, const x64_decoded_instruction& instruction) const
{
  if (instruction.rex_w)
  {
    context.set_unsupported_instruction(instruction.opcode);
    return x64_instruction_outcome::stop_running;
  }

  uint8_t secondary_opcode = 0;

  if (!context.read_u8(instruction.next_address, &secondary_opcode))
  {
    context.set_invalid_memory_access(instruction.next_address, 0);
    return x64_instruction_outcome::stop_running;
  }

  context.get_result().fault_opcode = secondary_opcode;
  return m_secondary_opcode_handlers[secondary_opcode](context, instruction);
}

