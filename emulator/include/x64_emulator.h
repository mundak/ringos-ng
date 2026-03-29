#pragma once

#include <stddef.h>
#include <stdint.h>

enum class x64_general_register : uint32_t
{
  RAX = 0,
  RCX = 1,
  RDX = 2,
  RBX = 3,
  RSP = 4,
  RBP = 5,
  RSI = 6,
  RDI = 7,
  R8 = 8,
  R9 = 9,
  R10 = 10,
  R11 = 11,
  R12 = 12,
  R13 = 13,
  R14 = 14,
  R15 = 15,
};

enum class x64_emulator_engine : uint32_t
{
  INTERPRETER = 0,
  JIT = 1,
};

enum class x64_emulator_completion : uint32_t
{
  THREAD_EXITED = 0,
  INSTRUCTION_LIMIT_REACHED = 1,
  INVALID_MEMORY_ACCESS = 2,
  UNSUPPORTED_INSTRUCTION = 3,
  INVALID_ARGUMENT = 4,
  UNSUPPORTED_ENGINE = 5,
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

