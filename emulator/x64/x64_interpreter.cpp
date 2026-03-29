#include "x64_interpreter.h"

#include "x64_decoder.h"

x64_interpreter::x64_interpreter(
  x64_emulator_state& state,
  const x64_emulator_memory& memory,
  const x64_emulator_callbacks& callbacks,
  const x64_emulator_options& options,
  x64_emulator_result& result)
  : m_context(state, memory, callbacks, options, result)
{
}

bool x64_interpreter::run()
{
  m_context.initialize_result();

  if (!m_context.is_valid_invocation())
  {
    m_context.get_result().completion = x64_emulator_completion::invalid_argument;
    return false;
  }

  while (m_context.get_result().retired_instructions < m_context.get_options().instruction_budget)
  {
    x64_decoded_instruction instruction {};

    if (!decode_x64_instruction(m_context, &instruction))
    {
      return true;
    }

    const x64_instruction_outcome outcome = m_dispatch.dispatch(m_context, instruction);

    if (outcome == x64_instruction_outcome::continue_running || outcome == x64_instruction_outcome::retire_and_stop)
    {
      ++m_context.get_result().retired_instructions;
    }

    if (outcome != x64_instruction_outcome::continue_running)
    {
      return true;
    }
  }

  m_context.get_result().completion = x64_emulator_completion::instruction_limit_reached;
  return true;
}

