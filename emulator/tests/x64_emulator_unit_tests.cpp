#include "user_space.h"
#include "x64_emulator.h"
#include "x64_pe64_image.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
  constexpr uintptr_t TEST_IMAGE_BASE = X64_USER_IMAGE_VIRTUAL_ADDRESS;
  constexpr size_t TEST_MEMORY_SIZE = 256;

  struct syscall_capture
  {
    const x64_emulator_memory* memory;
    uint64_t expected_number;
    uint64_t expected_argument0;
    int32_t return_status;
    bool stop_after_call;
    const char* expected_string;
    size_t call_count;
  };

  void fail_test(const char* test_name, const char* message)
  {
    std::fprintf(stderr, "FAIL: %s: %s\n", test_name, message);
  }

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

  int32_t capture_syscall(void* context, const x64_emulator_state& state, bool* out_should_continue)
  {
    syscall_capture& capture = *static_cast<syscall_capture*>(context);

    if (out_should_continue == nullptr)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    ++capture.call_count;
    const uint64_t syscall_number = state.general_registers[static_cast<uint32_t>(x64_general_register::rax)];
    const uint64_t argument0 = state.general_registers[static_cast<uint32_t>(x64_general_register::rdi)];

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

  bool run_program(
    const char* test_name,
    const uint8_t* program,
    size_t program_size,
    syscall_capture& capture,
    x64_emulator_result* out_result,
    x64_emulator_engine engine = x64_emulator_engine::interpreter,
    uint64_t instruction_budget = 64,
    uint64_t initial_rax = 0)
  {
    std::array<uint8_t, TEST_MEMORY_SIZE> memory_bytes {};

    if (program == nullptr)
    {
      fail_test(test_name, "program was null");
      return false;
    }

    if (program_size > memory_bytes.size())
    {
      fail_test(test_name, "program does not fit in test memory");
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
    state.general_registers[static_cast<uint32_t>(x64_general_register::rax)] = initial_rax;
    state.general_registers[static_cast<uint32_t>(x64_general_register::rsp)] = TEST_IMAGE_BASE + memory_bytes.size();
    capture.memory = &memory;
    capture.call_count = 0;
    const x64_emulator_callbacks callbacks {
      &capture,
      &capture_syscall,
    };
    const x64_emulator_options options {
      engine,
      instruction_budget,
    };

    if (!run_x64_emulator(state, memory, callbacks, options, out_result))
    {
      fail_test(test_name, "run_x64_emulator returned false");
      return false;
    }

    return true;
  }

  bool expect(bool condition, const char* test_name, const char* message)
  {
    if (!condition)
    {
      fail_test(test_name, message);
      return false;
    }

    return true;
  }

  bool test_mov_and_syscall()
  {
    constexpr std::array<uint8_t, 12> program {
      0xBF, 0x44, 0x33, 0x22, 0x11, 0xB8, 0x34, 0x12, 0x00, 0x00, 0x0F, 0x05,
    };
    syscall_capture capture {
      nullptr, 0x1234, 0x11223344, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_program("mov_and_syscall", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect(
             result.completion == x64_emulator_completion::thread_exited, "mov_and_syscall", "expected thread exit")
      && expect(capture.call_count == 1, "mov_and_syscall", "expected one syscall");
  }

  bool test_lea_rip_relative_string()
  {
    constexpr std::array<uint8_t, 12> program {
      0x48, 0x8D, 0x3D, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x05, 0x68, 0x69, 0x00,
    };
    syscall_capture capture {
      nullptr, 0, 0, STATUS_OK, true, "hi", 0,
    };
    x64_emulator_result result {};

    if (!run_program(
          "lea_rip_relative_string",
          program.data(),
          program.size(),
          capture,
          &result,
          x64_emulator_engine::interpreter,
          32,
          7))
    {
      return false;
    }

    return expect(
             result.completion == x64_emulator_completion::thread_exited,
             "lea_rip_relative_string",
             "expected thread exit")
      && expect(capture.call_count == 1, "lea_rip_relative_string", "expected one syscall");
  }

  bool test_call_and_ret()
  {
    constexpr std::array<uint8_t, 16> program {
      0xB8, 0x01, 0x00, 0x00, 0x00, 0xE8, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x05, 0x83, 0xC0, 0x05, 0xC3,
    };
    syscall_capture capture {
      nullptr, 6, 0, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_program("call_and_ret", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect(result.completion == x64_emulator_completion::thread_exited, "call_and_ret", "expected thread exit")
      && expect(capture.call_count == 1, "call_and_ret", "expected one syscall");
  }

  bool test_conditional_branch()
  {
    constexpr std::array<uint8_t, 29> program {
      0xB8, 0x01, 0x00, 0x00, 0x00, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x83, 0xF8, 0x01, 0x75, 0x07,
      0xBF, 0x07, 0x00, 0x00, 0x00, 0x0F, 0x05, 0xBF, 0x09, 0x00, 0x00, 0x00, 0x0F, 0x05,
    };
    syscall_capture capture {
      nullptr, 1, 7, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_program("conditional_branch", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect(
             result.completion == x64_emulator_completion::thread_exited, "conditional_branch", "expected thread exit")
      && expect(capture.call_count == 1, "conditional_branch", "expected one syscall");
  }

  bool test_xor_and_add()
  {
    constexpr std::array<uint8_t, 17> program {
      0xBF, 0x05, 0x00, 0x00, 0x00, 0x31, 0xFF, 0x83, 0xC7, 0x03, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x05,
    };
    syscall_capture capture {
      nullptr, 1, 3, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_program("xor_and_add", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect(result.completion == x64_emulator_completion::thread_exited, "xor_and_add", "expected thread exit")
      && expect(capture.call_count == 1, "xor_and_add", "expected one syscall");
  }

  bool test_instruction_budget_limit()
  {
    constexpr std::array<uint8_t, 2> program {
      0xEB,
      0xFE,
    };
    syscall_capture capture {
      nullptr, 0, 0, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_program(
          "instruction_budget_limit",
          program.data(),
          program.size(),
          capture,
          &result,
          x64_emulator_engine::interpreter,
          8))
    {
      return false;
    }

    return expect(
      result.completion == x64_emulator_completion::instruction_limit_reached,
      "instruction_budget_limit",
      "expected instruction budget exhaustion");
  }

  bool test_unsupported_engine_reports_cleanly()
  {
    constexpr std::array<uint8_t, 2> program {
      0x0F,
      0x05,
    };
    syscall_capture capture {
      nullptr, 0, 0, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_program(
          "unsupported_engine", program.data(), program.size(), capture, &result, x64_emulator_engine::jit, 8))
    {
      return false;
    }

    return expect(
      result.completion == x64_emulator_completion::unsupported_engine,
      "unsupported_engine",
      "expected unsupported engine completion");
  }

  bool test_loader_rejects_missing_header()
  {
    constexpr std::array<uint8_t, 1> image { 0x00 };
    std::array<uint8_t, X64_USER_REGION_SIZE> loaded_image {};
    x64_pe64_image_info image_info {};
    const x64_pe64_image_load_status status = load_x64_pe64_image(
      image.data(),
      image.size(),
      X64_USER_IMAGE_VIRTUAL_ADDRESS,
      loaded_image.data(),
      loaded_image.size(),
      &image_info);

    return expect(
      status == x64_pe64_image_load_status::missing_dos_header,
      "loader_rejects_missing_header",
      "expected missing DOS header status");
  }
}

int main()
{
  const std::vector<bool (*)()> tests {
    &test_mov_and_syscall,
    &test_lea_rip_relative_string,
    &test_call_and_ret,
    &test_conditional_branch,
    &test_xor_and_add,
    &test_instruction_budget_limit,
    &test_unsupported_engine_reports_cleanly,
    &test_loader_rejects_missing_header,
  };

  for (bool (*test)() : tests)
  {
    if (!test())
    {
      return 1;
    }
  }

  std::puts("PASS: x64 emulator unit tests");
  return 0;
}
