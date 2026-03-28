#pragma once

#include <stddef.h>
#include <stdint.h>

enum class x64_general_register : uint32_t
{
  rax = 0,
  rcx = 1,
  rdx = 2,
  rbx = 3,
  rsp = 4,
  rbp = 5,
  rsi = 6,
  rdi = 7,
  r8 = 8,
  r9 = 9,
  r10 = 10,
  r11 = 11,
  r12 = 12,
  r13 = 13,
  r14 = 14,
  r15 = 15,
};

enum class x64_emulator_engine : uint32_t
{
  interpreter = 0,
  jit = 1,
};

enum class x64_emulator_completion : uint32_t
{
  thread_exited = 0,
  instruction_limit_reached = 1,
  invalid_memory_access = 2,
  unsupported_instruction = 3,
  invalid_argument = 4,
  unsupported_engine = 5,
};

struct x64_emulator_state
{
  uint64_t general_registers[16];
  uintptr_t instruction_pointer;
  uint64_t flags;
};

struct x64_emulator_memory
{
  uintptr_t base_address;
  uint8_t* bytes;
  size_t size;
};

struct x64_emulator_callbacks
{
  void* context;
  int32_t (*handle_syscall)(void* context, const x64_emulator_state& state, bool* out_should_continue);
};

struct x64_emulator_options
{
  x64_emulator_engine engine;
  uint64_t instruction_budget;
};

struct x64_emulator_result
{
  x64_emulator_completion completion;
  uint64_t retired_instructions;
  uintptr_t fault_address;
  uint8_t fault_opcode;
};

bool run_x64_emulator(
  x64_emulator_state& state,
  const x64_emulator_memory& memory,
  const x64_emulator_callbacks& callbacks,
  const x64_emulator_options& options,
  x64_emulator_result* out_result);
const char* describe_x64_emulator_completion(x64_emulator_completion completion);
