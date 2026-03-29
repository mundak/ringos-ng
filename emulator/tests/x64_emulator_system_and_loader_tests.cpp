#include "x64_emulator_test_harness.h"

#include "x64_pe64_image.h"

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
          "unsupported_engine", program.data(), program.size(), capture, &result, x64_emulator_engine::jit, 8))
    {
      return false;
    }

    return expect_x64_emulator_test(
      result.completion == x64_emulator_completion::unsupported_engine,
      "unsupported_engine",
      "expected unsupported engine completion");
  }

  bool test_loader_rejects_missing_header()
  {
    constexpr std::array<uint8_t, 1> image {0x00};
    std::array<uint8_t, X64_USER_REGION_SIZE> loaded_image {};
    x64_pe64_image_info image_info {};
    const x64_pe64_image_load_status status = load_x64_pe64_image(
      image.data(),
      image.size(),
      X64_USER_IMAGE_VIRTUAL_ADDRESS,
      loaded_image.data(),
      loaded_image.size(),
      &image_info);

    return expect_x64_emulator_test(
      status == x64_pe64_image_load_status::missing_dos_header,
      "loader_rejects_missing_header",
      "expected missing DOS header status");
  }
}

void append_x64_system_and_loader_tests(std::vector<x64_emulator_test_case>& tests)
{
  tests.push_back({"unsupported_engine", &test_unsupported_engine_reports_cleanly});
  tests.push_back({"loader_rejects_missing_header", &test_loader_rejects_missing_header});
}

