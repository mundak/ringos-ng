#pragma once

namespace ringos
{

  // Halt the system with an error message. Writes the message to the serial
  // console and then spins indefinitely. Never returns.
  [[noreturn]] void panic(const char* message);

} // namespace ringos
