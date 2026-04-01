#pragma once

#include "x64_execution_context.h"
#include "x64_instruction_dispatch.h"

class x64_interpreter
{
public:
  x64_interpreter(
    x64_emulator_state& state,
    const x64_emulator_memory& memory,
    const x64_emulator_callbacks& callbacks,
    const x64_emulator_options& options,
    x64_emulator_result& result);

  bool run();

private:
  x64_execution_context m_context;
  x64_instruction_dispatch m_dispatch;
};
