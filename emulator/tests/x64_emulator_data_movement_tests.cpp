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
      0x48, 0x8D, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x8B, 0x39, 0xB8, 0x34, 0x12, 0x00, 0x00, 0x0F, 0x05, 0x44, 0x33, 0x22,
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
      0xBA, 0x44, 0x33, 0x22, 0x11, 0x48, 0x8D, 0x0D, 0x11, 0x00, 0x00, 0x00, 0x89, 0x11,
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

  bool test_mov_immediate64_memory_and_syscall()
  {
    constexpr std::array<uint8_t, 31> program {
      0x48, 0x8D, 0x0D, 0x11, 0x00, 0x00, 0x00, 0x48, 0xC7, 0x01, 0x44, 0x33, 0x22, 0x11, 0x48, 0x8B,
      0x39, 0xB8, 0x34, 0x12, 0x00, 0x00, 0x0F, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    x64_syscall_capture capture {
      nullptr, 0x1234, 0x11223344, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    if (!run_x64_emulator_test_program(
          "mov_immediate64_memory_and_syscall", program.data(), program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == X64_EMULATOR_COMPLETION_THREAD_EXITED,
             "mov_immediate64_memory_and_syscall",
             "expected thread exit")
      && expect_x64_emulator_test(
             capture.call_count == 1, "mov_immediate64_memory_and_syscall", "expected one syscall");
  }

  bool test_mov_immediate64_sib_memory_and_syscall()
  {
    constexpr std::array<uint8_t, 24> program {
      0x48, 0x83, 0xEC, 0x20, 0x48, 0xC7, 0x44, 0x24, 0x10, 0x44, 0x33, 0x22,
      0x11, 0x48, 0x8B, 0x7C, 0x24, 0x10, 0xB8, 0x34, 0x12, 0x00, 0x00, 0x0F,
    };
    constexpr std::array<uint8_t, 1> syscall_tail {
      0x05,
    };
    std::array<uint8_t, 25> full_program {};
    x64_syscall_capture capture {
      nullptr, 0x1234, 0x11223344, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    for (size_t index = 0; index < program.size(); ++index)
    {
      full_program[index] = program[index];
    }

    full_program[program.size()] = syscall_tail[0];

    if (!run_x64_emulator_test_program(
          "mov_immediate64_sib_memory_and_syscall", full_program.data(), full_program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == X64_EMULATOR_COMPLETION_THREAD_EXITED,
             "mov_immediate64_sib_memory_and_syscall",
             "expected thread exit")
      && expect_x64_emulator_test(
             capture.call_count == 1, "mov_immediate64_sib_memory_and_syscall", "expected one syscall");
  }

  bool test_strlen_style_loop_and_syscall()
  {
    constexpr std::array<uint8_t, 34> program {
      0x48, 0x8D, 0x0D, 0x1B, 0x00, 0x00, 0x00, 0x48, 0xC7, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0x80, 0x7C, 0x01,
      0x01, 0x00, 0x48, 0x8D, 0x40, 0x01, 0x75, 0xF5, 0x89, 0xC7, 0xB8, 0x34, 0x12, 0x00, 0x00, 0x0F, 0x05,
    };
    constexpr std::array<uint8_t, 3> string_data { 'h', 'i', '\0' };
    std::array<uint8_t, 37> full_program {};
    x64_syscall_capture capture {
      nullptr, 0x1234, 2, STATUS_OK, true, nullptr, 0,
    };
    x64_emulator_result result {};

    for (size_t index = 0; index < program.size(); ++index)
    {
      full_program[index] = program[index];
    }

    for (size_t index = 0; index < string_data.size(); ++index)
    {
      full_program[program.size() + index] = string_data[index];
    }

    if (!run_x64_emulator_test_program(
          "strlen_style_loop_and_syscall", full_program.data(), full_program.size(), capture, &result))
    {
      return false;
    }

    return expect_x64_emulator_test(
             result.completion == X64_EMULATOR_COMPLETION_THREAD_EXITED,
             "strlen_style_loop_and_syscall",
             "expected thread exit")
      && expect_x64_emulator_test(capture.call_count == 1, "strlen_style_loop_and_syscall", "expected one syscall");
  }
}

void append_x64_data_movement_tests(std::vector<x64_emulator_test_case>& tests)
{
  tests.push_back({ "mov_register_and_syscall", &test_mov_register_and_syscall });
  tests.push_back({ "mov_and_syscall", &test_mov_and_syscall });
  tests.push_back({ "lea_rip_relative_string", &test_lea_rip_relative_string });
  tests.push_back({ "mov_memory_to_register_and_syscall", &test_mov_memory_to_register_and_syscall });
  tests.push_back({ "mov_register_to_memory_and_syscall", &test_mov_register_to_memory_and_syscall });
  tests.push_back({ "mov_immediate64_memory_and_syscall", &test_mov_immediate64_memory_and_syscall });
  tests.push_back({ "mov_immediate64_sib_memory_and_syscall", &test_mov_immediate64_sib_memory_and_syscall });
  tests.push_back({ "strlen_style_loop_and_syscall", &test_strlen_style_loop_and_syscall });
}
