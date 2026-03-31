#pragma once

#include <stddef.h>
#include <stdint.h>

enum x64_general_register : uint32_t
{
  X64_GENERAL_REGISTER_RAX = 0,
  X64_GENERAL_REGISTER_RCX = 1,
  X64_GENERAL_REGISTER_RDX = 2,
  X64_GENERAL_REGISTER_RBX = 3,
  X64_GENERAL_REGISTER_RSP = 4,
  X64_GENERAL_REGISTER_RBP = 5,
  X64_GENERAL_REGISTER_RSI = 6,
  X64_GENERAL_REGISTER_RDI = 7,
  X64_GENERAL_REGISTER_R8 = 8,
  X64_GENERAL_REGISTER_R9 = 9,
  X64_GENERAL_REGISTER_R10 = 10,
  X64_GENERAL_REGISTER_R11 = 11,
  X64_GENERAL_REGISTER_R12 = 12,
  X64_GENERAL_REGISTER_R13 = 13,
  X64_GENERAL_REGISTER_R14 = 14,
  X64_GENERAL_REGISTER_R15 = 15,
};

enum x64_emulator_engine : uint32_t
{
  X64_EMULATOR_ENGINE_INTERPRETER = 0,
  X64_EMULATOR_ENGINE_JIT = 1,
};

enum x64_emulator_completion : uint32_t
{
  X64_EMULATOR_COMPLETION_THREAD_EXITED = 0,
  X64_EMULATOR_COMPLETION_INSTRUCTION_LIMIT_REACHED = 1,
  X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS = 2,
  X64_EMULATOR_COMPLETION_UNSUPPORTED_INSTRUCTION = 3,
  X64_EMULATOR_COMPLETION_INVALID_ARGUMENT = 4,
  X64_EMULATOR_COMPLETION_UNSUPPORTED_ENGINE = 5,
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

