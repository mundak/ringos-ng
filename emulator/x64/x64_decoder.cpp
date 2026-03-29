#include "x64_decoder.h"

#include "x64_execution_context.h"

bool decode_x64_instruction(x64_execution_context& context, x64_decoded_instruction* out_instruction)
{
  if (out_instruction == nullptr)
  {
    return false;
  }

  x64_decoded_instruction instruction {
    context.get_state().instruction_pointer,
    context.get_state().instruction_pointer,
    context.get_state().instruction_pointer,
    0,
    false,
  };

  if (!context.read_u8(instruction.opcode_address, &instruction.opcode))
  {
    context.set_invalid_memory_access(instruction.opcode_address, 0);
    return false;
  }

  if (instruction.opcode == 0x48)
  {
    instruction.rex_w = true;
    ++instruction.opcode_address;

    if (!context.read_u8(instruction.opcode_address, &instruction.opcode))
    {
      context.set_invalid_memory_access(instruction.opcode_address, 0);
      return false;
    }
  }

  instruction.next_address = instruction.opcode_address + 1;
  *out_instruction = instruction;
  return true;
}

