#include "x64_emulator_test_harness.h"
#include "x64_pe64_image.h"

#include <array>

namespace
{
  bool test_call_and_ret()
  {
    constexpr std::array<uint8_t, 16> program {
      0xB8, 0x01, 0x00, 0x00, 0x00, 0xE8, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x05, 0x83, 0xC0, 0x05, 0xC3,
    };
    x64_syscall_capture capture {
      nullptr, 6, 0, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program("call_and_ret", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == x64_emulator_completion::THREAD_EXITED, "call_and_ret", "expected thread exit")
      && expect_x64_emulator_test(capture.call_count == 1, "call_and_ret", "expected one syscall");
  }

  bool test_conditional_branch()
  {
    constexpr std::array<uint8_t, 29> program {
      0xB8, 0x01, 0x00, 0x00, 0x00, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x83, 0xF8, 0x01, 0x75, 0x07,
      0xBF, 0x07, 0x00, 0x00, 0x00, 0x0F, 0x05, 0xBF, 0x09, 0x00, 0x00, 0x00, 0x0F, 0x05,
    };
    x64_syscall_capture capture {
      nullptr, 1, 7, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program("conditional_branch", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == x64_emulator_completion::THREAD_EXITED, "conditional_branch", "expected thread exit")
      && expect_x64_emulator_test(capture.call_count == 1, "conditional_branch", "expected one syscall");
  }

  bool test_call_indirect_rip_relative()
  {
    constexpr uintptr_t indirect_target = X64_USER_IMAGE_VIRTUAL_ADDRESS + 24;
    constexpr std::array<uint8_t, 25> program {
      0xFF,
      0x15,
      0x0A,
      0x00,
      0x00,
      0x00,
      0xB8,
      0x01,
      0x00,
      0x00,
      0x00,
      0x0F,
      0x05,
      0x90,
      0x90,
      0x90,
      static_cast<uint8_t>(indirect_target & 0xFF),
      static_cast<uint8_t>((indirect_target >> 8) & 0xFF),
      static_cast<uint8_t>((indirect_target >> 16) & 0xFF),
      static_cast<uint8_t>((indirect_target >> 24) & 0xFF),
      0x00,
      0x00,
      0x00,
      0x00,
      0xC3,
    };
    x64_syscall_capture capture {
      nullptr, 1, 0, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program("call_indirect_rip_relative", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == x64_emulator_completion::THREAD_EXITED,
             "call_indirect_rip_relative",
             "expected thread exit")
      && expect_x64_emulator_test(capture.call_count == 1, "call_indirect_rip_relative", "expected one syscall");
  }

  bool test_instruction_budget_limit()
  {
    constexpr std::array<uint8_t, 2> program {
      0xEB,
      0xFE,
    };
    x64_syscall_capture capture {
      nullptr, 0, 0, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program(
          "instruction_budget_limit",
          program.data(),
          program.size(),
          capture,
          &result,
          x64_emulator_engine::INTERPRETER,
          8))
    {
      return false;
    }

    return expect_x64_emulator_test(
      result.completion == x64_emulator_completion::INSTRUCTION_LIMIT_REACHED,
      "instruction_budget_limit",
      "expected instruction budget exhaustion");
  }
}

void append_x64_stack_and_control_flow_tests(std::vector<x64_emulator_test_case>& tests)
{
  tests.push_back({ "call_and_ret", &test_call_and_ret });
  tests.push_back({ "conditional_branch", &test_conditional_branch });
  tests.push_back({ "call_indirect_rip_relative", &test_call_indirect_rip_relative });
  tests.push_back({ "instruction_budget_limit", &test_instruction_budget_limit });
}

