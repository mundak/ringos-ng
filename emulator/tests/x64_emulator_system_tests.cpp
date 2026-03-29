#include "x64_emulator_test_harness.h"

#include <array>

namespace
{
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
          "unsupported_engine", program.data(), program.size(), capture, &result, x64_emulator_engine::JIT, 8))
    {
      return false;
    }

    return expect_x64_emulator_test(
      result.completion == x64_emulator_completion::UNSUPPORTED_ENGINE,
      "unsupported_engine",
      "expected unsupported engine completion");
  }
}

void append_x64_system_tests(std::vector<x64_emulator_test_case>& tests)
{
  tests.push_back({ "unsupported_engine", &test_unsupported_engine_reports_cleanly });
}

