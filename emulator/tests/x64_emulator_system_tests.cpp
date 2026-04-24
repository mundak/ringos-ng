#include "x64_emulator_test_harness.h"

#include <array>

namespace
{
  bool test_syscall_yield_reports_cleanly()
  {
    constexpr std::array<uint8_t, 2> program {
      0x0F,
      0x05,
    };
    x64_syscall_capture capture {
      nullptr, 0, 0, STATUS_OK, true, nullptr, 0, X64_EMULATOR_SYSCALL_ACTION_YIELD,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program("syscall_yield", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             did_x64_emulator_yield(result), "syscall_yield", "expected syscall-triggered yield completion")
      && expect_x64_emulator_test(capture.call_count == 1, "syscall_yield", "expected a single syscall");
  }

  bool test_unsupported_engine_reports_cleanly()
  {
    constexpr std::array<uint8_t, 2> program {
      0x0F,
      0x05,
    };
    x64_syscall_capture capture {
      nullptr, 0, 0, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program(
          "unsupported_engine", program.data(), program.size(), capture, &result, X64_EMULATOR_ENGINE_JIT, 8))
    {
      return false;
    }

    return expect_x64_emulator_test(
      did_x64_emulator_fail_with_backend(result, X64_EMULATOR_BACKEND_FAILURE_UNSUPPORTED_ENGINE),
      "unsupported_engine",
      "expected unsupported engine completion");
  }

  bool test_invalid_memory_access_reports_guest_fault()
  {
    constexpr std::array<uint8_t, 1> program {
      0xC3,
    };
    x64_syscall_capture capture {
      nullptr, 0, 0, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program("invalid_memory_access", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             did_x64_emulator_stop_for_guest_fault(result, X64_EMULATOR_GUEST_STOP_REASON_INVALID_MEMORY_ACCESS),
             "invalid_memory_access",
             "expected invalid memory access guest fault")
      && expect_x64_emulator_test(
             result.fault.instruction_pointer == X64_TEST_PROGRAM_BASE,
             "invalid_memory_access",
             "expected fault instruction pointer")
      && expect_x64_emulator_test(
             result.fault.memory_address == X64_TEST_PROGRAM_BASE + X64_TEST_PROGRAM_MEMORY_SIZE,
             "invalid_memory_access",
             "expected fault memory address")
      && expect_x64_emulator_test(result.fault.opcode == 0xC3, "invalid_memory_access", "expected ret opcode");
  }

  bool test_unsupported_instruction_reports_guest_fault()
  {
    constexpr std::array<uint8_t, 1> program {
      0x06,
    };
    x64_syscall_capture capture {
      nullptr, 0, 0, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program("unsupported_instruction", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             did_x64_emulator_stop_for_guest_fault(result, X64_EMULATOR_GUEST_STOP_REASON_UNSUPPORTED_INSTRUCTION),
             "unsupported_instruction",
             "expected unsupported instruction guest fault")
      && expect_x64_emulator_test(
             result.fault.instruction_pointer == X64_TEST_PROGRAM_BASE,
             "unsupported_instruction",
             "expected fault instruction pointer")
      && expect_x64_emulator_test(
             result.fault.memory_address == 0,
             "unsupported_instruction",
             "expected no memory address for unsupported instruction")
      && expect_x64_emulator_test(
             result.fault.opcode == 0x06, "unsupported_instruction", "expected unsupported opcode");
  }
}

void append_x64_system_tests(std::vector<x64_emulator_test_case>& tests)
{
  tests.push_back({ "syscall_yield", &test_syscall_yield_reports_cleanly });
  tests.push_back({ "unsupported_engine", &test_unsupported_engine_reports_cleanly });
  tests.push_back({ "invalid_memory_access", &test_invalid_memory_access_reports_guest_fault });
  tests.push_back({ "unsupported_instruction", &test_unsupported_instruction_reports_guest_fault });
}
