#pragma once

// Write a debug message to the serial console with a standard prefix.
void debug_log(const char* message);

// Trigger an architecture-specific breakpoint trap for an attached debugger.
void debug_break();

// Emit a debug message, then trigger an architecture-specific breakpoint trap.
void debug_break(const char* reason);
