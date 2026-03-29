#include "x64_emulator_test_harness.h"

#include <cstdio>
#include <vector>

int main()
{
  std::vector<x64_emulator_test_case> tests;
  append_x64_data_movement_tests(tests);
  append_x64_stack_and_control_flow_tests(tests);
  append_x64_alu_tests(tests);
  append_x64_system_and_loader_tests(tests);

  for (const x64_emulator_test_case& test_case : tests)
  {
    if (!test_case.run())
    {
      return 1;
    }
  }

  std::puts("PASS: x64 emulator unit tests");
  return 0;
}
