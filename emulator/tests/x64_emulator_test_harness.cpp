#include "x64_emulator_test_harness.h"

#include "user_space.h"
#include "x64_pe64_image.h"

#include <array>
#include <cstdio>
#include <cstring>

namespace
{
  constexpr uintptr_t TEST_IMAGE_BASE = X64_USER_IMAGE_VIRTUAL_ADDRESS;
  constexpr size_t TEST_MEMORY_SIZE = 256;

  uint8_t* translate_guest(const x64_emulator_memory& memory, uintptr_t guest_address, size_t length)
  {
    if (guest_address < memory.base_address)
    {
      return nullptr;
    }

    const uintptr_t offset = guest_address - memory.base_address;

    if (offset > memory.size || length > memory.size - static_cast<size_t>(offset))
    {
      return nullptr;
    }

    return memory.bytes + offset;
  }

  int32_t capture_x64_syscall(void* context, const x64_emulator_state& state, bool* out_should_continue)
  {
    x64_syscall_capture& capture = *static_cast<x64_syscall_capture*>(context);

    if (out_should_continue == nullptr)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    ++capture.call_count;
    const uint64_t syscall_number = state.general_registers[static_cast<uint32_t>(x64_general_register::RAX)];
    const uint64_t argument0 = state.general_registers[static_cast<uint32_t>(x64_general_register::RDI)];

    if (syscall_number != capture.expected_number)
    {
      *out_should_continue = false;
      return STATUS_BAD_STATE;
    }

    if (capture.expected_string != nullptr)
    {
      uint8_t* string_bytes = translate_guest(*capture.memory, static_cast<uintptr_t>(argument0), 1);

      if (
        string_bytes == nullptr
        || std::strcmp(reinterpret_cast<const char*>(string_bytes), capture.expected_string) != 0)
      {
        *out_should_continue = false;
        return STATUS_FAULT;
      }
    }
    else if (argument0 != capture.expected_argument0)
    {
      *out_should_continue = false;
      return STATUS_INVALID_ARGUMENT;
    }

    *out_should_continue = !capture.stop_after_call;
    return capture.return_status;
  }
}

void fail_x64_emulator_test(const char* test_name, const char* message)
{
  std::fprintf(stderr, "FAIL: %s: %s\n", test_name, message);
}

bool expect_x64_emulator_test(bool condition, const char* test_name, const char* message)
{
  if (!condition)
  {
    fail_x64_emulator_test(test_name, message);
    return false;
  }

  return true;
}

bool run_x64_emulator_test_program(
  const char* test_name,
  const uint8_t* program,
  size_t program_size,
  x64_syscall_capture& capture,
  x64_emulator_result* out_result,
  x64_emulator_engine engine,
  uint64_t instruction_budget,
  uint64_t initial_rax)
{
  std::array<uint8_t, TEST_MEMORY_SIZE> memory_bytes {};

  if (program == nullptr)
  {
    fail_x64_emulator_test(test_name, "program was null");
    return false;
  }

  if (program_size > memory_bytes.size())
  {
    fail_x64_emulator_test(test_name, "program does not fit in test memory");
    return false;
  }

  std::memcpy(memory_bytes.data(), program, program_size);

  x64_emulator_memory memory {
    TEST_IMAGE_BASE,
    memory_bytes.data(),
    memory_bytes.size(),
  };
  x64_emulator_state state {};
  state.instruction_pointer = TEST_IMAGE_BASE;
  state.flags = 0x202;
  state.general_registers[static_cast<uint32_t>(x64_general_register::RAX)] = initial_rax;
  state.general_registers[static_cast<uint32_t>(x64_general_register::RSP)] = TEST_IMAGE_BASE + memory_bytes.size();
  capture.memory = &memory;
  capture.call_count = 0;
  const x64_emulator_callbacks callbacks {
    &capture,
    &capture_x64_syscall,
  };
  const x64_emulator_options options {
    engine,
    instruction_budget,
  };

  if (!run_x64_emulator(state, memory, callbacks, options, out_result))
  {
    fail_x64_emulator_test(test_name, "run_x64_emulator returned false");
    return false;
  }

  return true;
}

