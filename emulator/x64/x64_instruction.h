#pragma once

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <cstdint>
#else
#include <stdint.h>
#endif

class x64_execution_context;

enum x64_instruction_outcome : uint32_t
{
  X64_INSTRUCTION_OUTCOME_CONTINUE_RUNNING = 0,
  X64_INSTRUCTION_OUTCOME_RETIRE_AND_STOP = 1,
  X64_INSTRUCTION_OUTCOME_STOP_RUNNING = 2,
};

struct x64_decoded_instruction
{
  uintptr_t address;
  uintptr_t opcode_address;
  uintptr_t next_address;
  uint8_t opcode;
  bool rex_w;
  bool rex_r;
  bool rex_x;
  bool rex_b;
};

using x64_instruction_handler
  = x64_instruction_outcome (*)(x64_execution_context& context, const x64_decoded_instruction& instruction);

