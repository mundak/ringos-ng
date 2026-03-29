#include "x64_emulator.h"

#include "x64_interpreter.h"

bool run_x64_emulator(
  x64_emulator_state& state,
  const x64_emulator_memory& memory,
  const x64_emulator_callbacks& callbacks,
  const x64_emulator_options& options,
  x64_emulator_result* out_result)
{
  if (out_result == nullptr)
  {
    return false;
  }

  if (options.engine != x64_emulator_engine::interpreter)
  {
    *out_result = {
      x64_emulator_completion::unsupported_engine,
      0,
      state.instruction_pointer,
      0,
    };
    return true;
  }

  x64_interpreter interpreter(state, memory, callbacks, options, *out_result);
  return interpreter.run();
}

const char* describe_x64_emulator_completion(x64_emulator_completion completion)
{
  switch (completion)
  {
  case x64_emulator_completion::thread_exited:
    return "x64 emulator thread exited cleanly";
  case x64_emulator_completion::instruction_limit_reached:
    return "x64 emulator hit the instruction budget";
  case x64_emulator_completion::invalid_memory_access:
    return "x64 emulator touched unmapped guest memory";
  case x64_emulator_completion::unsupported_instruction:
    return "x64 emulator hit an unsupported instruction";
  case x64_emulator_completion::invalid_argument:
    return "x64 emulator received an invalid argument";
  case x64_emulator_completion::unsupported_engine:
    return "x64 emulator backend is not implemented";
  }

  return "x64 emulator failed with an unknown completion state";
}
