#pragma once

#include "x64_instruction.h"

class x64_execution_context;

class x64_instruction_dispatch
{
public:
  x64_instruction_dispatch();

  x64_instruction_outcome dispatch(x64_execution_context& context, const x64_decoded_instruction& instruction) const;

private:
  void initialize_opcode_handlers();
  x64_instruction_outcome dispatch_secondary_opcode(
    x64_execution_context& context, const x64_decoded_instruction& instruction) const;

  x64_instruction_handler m_primary_opcode_handlers[256];
  x64_instruction_handler m_secondary_opcode_handlers[256];
};

