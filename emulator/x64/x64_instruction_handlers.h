#pragma once

#include "x64_instruction.h"

x64_instruction_outcome execute_x64_unsupported(
  x64_execution_context& context, const x64_decoded_instruction& instruction);

x64_instruction_outcome execute_x64_push_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_pop_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction);

x64_instruction_outcome execute_x64_mov_immediate32(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_mov_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_lea_rip_relative(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_nop(x64_execution_context& context, const x64_decoded_instruction& instruction);

x64_instruction_outcome execute_x64_xor_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_group83(x64_execution_context& context, const x64_decoded_instruction& instruction);

x64_instruction_outcome execute_x64_syscall(x64_execution_context& context, const x64_decoded_instruction& instruction);

x64_instruction_outcome execute_x64_return(x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_call_relative(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_call_indirect_memory(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_jump_relative_near(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_jump_relative_short(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_jump_short_condition(
  x64_execution_context& context, const x64_decoded_instruction& instruction);

