#pragma once

#include "x64_instruction.h"

x64_instruction_outcome execute_x64_unsupported(
  x64_execution_context& context, const x64_decoded_instruction& instruction);

x64_instruction_outcome execute_x64_push_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_or_register_byte(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_or_rm_to_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_pop_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction);

x64_instruction_outcome execute_x64_mov_immediate32(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_mov_immediate_to_rm(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_mov_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_lea(x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_secondary_setcc(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_secondary_movzx_byte(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_nop(x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_secondary_nop(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_secondary_mov_xmm(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_secondary_store_xmm(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_secondary_xorps(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_secondary_movd(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_secondary_punpcklbw(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_secondary_shuffle(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_secondary_movq_store(
  x64_execution_context& context, const x64_decoded_instruction& instruction);

x64_instruction_outcome execute_x64_xor_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_test_accumulator_immediate(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_test_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_group_f6(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_group_f7(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_group80(x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_group81(x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_group83(x64_execution_context& context, const x64_decoded_instruction& instruction);
x64_instruction_outcome execute_x64_compare_rm_with_register(
  x64_execution_context& context, const x64_decoded_instruction& instruction);

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
x64_instruction_outcome execute_x64_jump_near_condition(
  x64_execution_context& context, const x64_decoded_instruction& instruction);
