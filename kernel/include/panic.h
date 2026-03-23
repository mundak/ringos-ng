#pragma once

// Halt the system with an error message. Never returns.
[[noreturn]] void panic(const char* message);
