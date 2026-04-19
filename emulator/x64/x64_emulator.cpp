#include "x64_emulator.h"

#include "x64_interpreter.h"

namespace
{
  void initialize_backend_failure_result(
    x64_emulator_result* out_result, x64_emulator_state& state, x64_emulator_backend_failure failure)
  {
    if (out_result == nullptr)
    {
      return;
    }

    x64_emulator_completion completion = X64_EMULATOR_COMPLETION_UNSUPPORTED_ENGINE;

    switch (failure)
    {
    case X64_EMULATOR_BACKEND_FAILURE_INSTRUCTION_LIMIT_REACHED:
      completion = X64_EMULATOR_COMPLETION_INSTRUCTION_LIMIT_REACHED;
      break;
    case X64_EMULATOR_BACKEND_FAILURE_INVALID_ARGUMENT:
      completion = X64_EMULATOR_COMPLETION_INVALID_ARGUMENT;
      break;
    case X64_EMULATOR_BACKEND_FAILURE_UNSUPPORTED_ENGINE:
      completion = X64_EMULATOR_COMPLETION_UNSUPPORTED_ENGINE;
      break;
    case X64_EMULATOR_BACKEND_FAILURE_NONE:
      completion = X64_EMULATOR_COMPLETION_THREAD_EXITED;
      break;
    }

    *out_result = {
      completion,
      X64_EMULATOR_GUEST_STOP_REASON_NONE,
      failure,
      0,
      state.instruction_pointer,
      0,
      {
        state.instruction_pointer,
        0,
        0,
      },
    };
  }
}

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
    initialize_backend_failure_result(out_result, state, X64_EMULATOR_BACKEND_FAILURE_UNSUPPORTED_ENGINE);
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

const char* describe_x64_emulator_guest_stop_reason(x64_emulator_guest_stop_reason reason)
{
  switch (reason)
  {
  case X64_EMULATOR_GUEST_STOP_REASON_NONE:
    return "x64 emulator stopped without a guest event";
  case X64_EMULATOR_GUEST_STOP_REASON_THREAD_EXITED:
    return "x64 emulator thread exited cleanly";
  case X64_EMULATOR_GUEST_STOP_REASON_INVALID_MEMORY_ACCESS:
    return "x64 emulator touched unmapped guest memory";
  case X64_EMULATOR_GUEST_STOP_REASON_UNSUPPORTED_INSTRUCTION:
    return "x64 emulator hit an unsupported instruction";
  }

  return "x64 emulator failed with an unknown guest stop reason";
}

const char* describe_x64_emulator_backend_failure(x64_emulator_backend_failure failure)
{
  switch (failure)
  {
  case X64_EMULATOR_BACKEND_FAILURE_NONE:
    return "x64 emulator backend did not fail";
  case X64_EMULATOR_BACKEND_FAILURE_INSTRUCTION_LIMIT_REACHED:
    return "x64 emulator hit the instruction budget";
  case X64_EMULATOR_BACKEND_FAILURE_INVALID_ARGUMENT:
    return "x64 emulator received an invalid argument";
  case X64_EMULATOR_BACKEND_FAILURE_UNSUPPORTED_ENGINE:
    return "x64 emulator backend is not implemented";
  }

  return "x64 emulator failed with an unknown backend failure";
}
