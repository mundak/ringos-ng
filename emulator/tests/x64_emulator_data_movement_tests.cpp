#include "x64_emulator_test_harness.h"

#include <array>

namespace
{
  bool test_mov_register_and_syscall()
  {
    constexpr std::array<uint8_t, 15> program {
      0xBA, 0x44, 0x33, 0x22, 0x11, 0x48, 0x89, 0xD7, 0xB8, 0x34, 0x12, 0x00, 0x00, 0x0F, 0x05,
    };
    x64_syscall_capture capture {
      nullptr, 0x1234, 0x11223344, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program("mov_register_and_syscall", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == X64_EMULATOR_COMPLETION_THREAD_EXITED,
             "mov_register_and_syscall",
             "expected thread exit")
      && expect_x64_emulator_test(capture.call_count == 1, "mov_register_and_syscall", "expected one syscall");
  }

  bool test_mov_and_syscall()
  {
    constexpr std::array<uint8_t, 12> program {
      0xBF, 0x44, 0x33, 0x22, 0x11, 0xB8, 0x34, 0x12, 0x00, 0x00, 0x0F, 0x05,
    };
    x64_syscall_capture capture {
      nullptr, 0x1234, 0x11223344, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program("mov_and_syscall", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == X64_EMULATOR_COMPLETION_THREAD_EXITED, "mov_and_syscall", "expected thread exit")
      && expect_x64_emulator_test(capture.call_count == 1, "mov_and_syscall", "expected one syscall");
  }

  bool test_lea_rip_relative_string()
  {
    constexpr std::array<uint8_t, 12> program {
      0x48, 0x8D, 0x3D, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x05, 0x68, 0x69, 0x00,
    };
    x64_syscall_capture capture {
      nullptr, 0, 0, STATUS_OK, true, "hi", 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program(
          "lea_rip_relative_string",
          program.data(),
          program.size(),
          capture,
          &result,
          X64_EMULATOR_ENGINE_INTERPRETER,
          32,
          7))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == X64_EMULATOR_COMPLETION_THREAD_EXITED,
             "lea_rip_relative_string",
             "expected thread exit")
      && expect_x64_emulator_test(capture.call_count == 1, "lea_rip_relative_string", "expected one syscall");
  }

  bool test_mov_memory_to_register_and_syscall()
  {
    constexpr std::array<uint8_t, 19> program {
      0x48, 0x8D, 0x0D, 0x05, 0x00, 0x00, 0x00, 0x8B, 0x39, 0xB8, 0x34, 0x12, 0x00, 0x00, 0x0F, 0x05, 0x44, 0x33, 0x22,
    };
    x64_syscall_capture capture {
      nullptr, 0x1234, 0x00223344, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program(
          "mov_memory_to_register_and_syscall", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == X64_EMULATOR_COMPLETION_THREAD_EXITED,
             "mov_memory_to_register_and_syscall",
             "expected thread exit")
      && expect_x64_emulator_test(
             capture.call_count == 1, "mov_memory_to_register_and_syscall", "expected one syscall");
  }

  bool test_mov_register_to_memory_and_syscall()
  {
    constexpr std::array<uint8_t, 28> program {
      0xBA, 0x44, 0x33, 0x22, 0x11, 0x48, 0x8D, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x89, 0x11,
      0x8B, 0x39, 0xB8, 0x34, 0x12, 0x00, 0x00, 0x0F, 0x05, 0x00, 0x00, 0x00, 0x00, 0x90,
    };
    x64_syscall_capture capture {
      nullptr, 0x1234, 0x11223344, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program(
          "mov_register_to_memory_and_syscall", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == X64_EMULATOR_COMPLETION_THREAD_EXITED,
             "mov_register_to_memory_and_syscall",
             "expected thread exit")
      && expect_x64_emulator_test(
             capture.call_count == 1, "mov_register_to_memory_and_syscall", "expected one syscall");
  }
}

void append_x64_data_movement_tests(std::vector<x64_emulator_test_case>& tests)
{
  tests.push_back({ "mov_register_and_syscall", &test_mov_register_and_syscall });
  tests.push_back({ "mov_and_syscall", &test_mov_and_syscall });
  tests.push_back({ "lea_rip_relative_string", &test_lea_rip_relative_string });
  tests.push_back({ "mov_memory_to_register_and_syscall", &test_mov_memory_to_register_and_syscall });
  tests.push_back({ "mov_register_to_memory_and_syscall", &test_mov_register_to_memory_and_syscall });
}

