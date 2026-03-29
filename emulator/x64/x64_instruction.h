#pragma once

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <cstdint>
#else
#include <stdint.h>
#endif

class x64_execution_context;

enum class x64_instruction_outcome : uint32_t
{
  continue_running = 0,
  retire_and_stop = 1,
  stop_running = 2,
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
