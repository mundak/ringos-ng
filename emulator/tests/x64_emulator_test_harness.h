#pragma once

#include "user_space.h"
#include "x64_emulator.h"

#include <vector>

struct x64_emulator_test_case
{
  const char* name;
  bool (*run)();
};

inline constexpr uintptr_t X64_TEST_PROGRAM_BASE = 0x400000;

struct x64_syscall_capture
{
  const x64_emulator_memory* memory;
  uint64_t expected_number;
  uint64_t expected_argument0;
  int32_t return_status;
  bool stop_after_call;
  const char* expected_string;
  size_t call_count;
};

void fail_x64_emulator_test(const char* test_name, const char* message);
bool expect_x64_emulator_test(bool condition, const char* test_name, const char* message);
bool run_x64_emulator_test_program(
  const char* test_name,
  const uint8_t* program,
  size_t program_size,
  x64_syscall_capture& capture,
  x64_emulator_result* out_result,
  x64_emulator_engine engine = x64_emulator_engine::INTERPRETER,
  uint64_t instruction_budget = 64,
  uint64_t initial_rax = 0);

void append_x64_data_movement_tests(std::vector<x64_emulator_test_case>& tests);
void append_x64_stack_and_control_flow_tests(std::vector<x64_emulator_test_case>& tests);
void append_x64_alu_tests(std::vector<x64_emulator_test_case>& tests);
void append_x64_system_tests(std::vector<x64_emulator_test_case>& tests);

