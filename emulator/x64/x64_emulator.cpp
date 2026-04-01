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

  if (options.engine != X64_EMULATOR_ENGINE_INTERPRETER)
  {
    *out_result = {
      X64_EMULATOR_COMPLETION_UNSUPPORTED_ENGINE,
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
  case X64_EMULATOR_COMPLETION_THREAD_EXITED:
    return "x64 emulator thread exited cleanly";
  case X64_EMULATOR_COMPLETION_INSTRUCTION_LIMIT_REACHED:
    return "x64 emulator hit the instruction budget";
  case X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS:
    return "x64 emulator touched unmapped guest memory";
  case X64_EMULATOR_COMPLETION_UNSUPPORTED_INSTRUCTION:
    return "x64 emulator hit an unsupported instruction";
  case X64_EMULATOR_COMPLETION_INVALID_ARGUMENT:
    return "x64 emulator received an invalid argument";
  case X64_EMULATOR_COMPLETION_UNSUPPORTED_ENGINE:
    return "x64 emulator backend is not implemented";
  }

  return "x64 emulator failed with an unknown completion state";
}
