#include "x64_emulator_test_harness.h"

#include <array>

namespace
{
  bool test_xor_and_add()
  {
    constexpr std::array<uint8_t, 17> program {
      0xBF, 0x05, 0x00, 0x00, 0x00, 0x31, 0xFF, 0x83, 0xC7, 0x03, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x05,
    };
    x64_syscall_capture capture {
      nullptr, 1, 3, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program("xor_and_add", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == x64_emulator_completion::THREAD_EXITED, "xor_and_add", "expected thread exit")
      && expect_x64_emulator_test(capture.call_count == 1, "xor_and_add", "expected one syscall");
  }

  bool test_test_register_sets_zero_flag_for_je()
  {
    constexpr std::array<uint8_t, 29> program {
      0xB9, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC9, 0x74, 0x07, 0xBF, 0x11, 0x00, 0x00, 0x00,
      0xEB, 0x05, 0xBF, 0x22, 0x00, 0x00, 0x00, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x05,
    };
    x64_syscall_capture capture {
      nullptr, 1, 0x22, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program(
          "test_register_sets_zero_flag_for_je", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == x64_emulator_completion::THREAD_EXITED,
             "test_register_sets_zero_flag_for_je",
             "expected thread exit")
      && expect_x64_emulator_test(
             capture.call_count == 1, "test_register_sets_zero_flag_for_je", "expected one syscall");
  }
}

void append_x64_alu_tests(std::vector<x64_emulator_test_case>& tests)
{
  tests.push_back({ "xor_and_add", &test_xor_and_add });
  tests.push_back({ "test_register_sets_zero_flag_for_je", &test_test_register_sets_zero_flag_for_je });
}
