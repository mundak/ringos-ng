#pragma once

// Write a debug message to the host-side debug channel with a standard prefix.
// On arm64 under scripts/debug-arm64.sh this is routed to the attached GDB
// session via semihosting. On x64 under scripts/debug-x64.sh this is routed to
// QEMU's debug console.
void debug_log(const char* message);

// Trigger an architecture-specific breakpoint trap for an attached debugger.
void debug_break();

// Emit a debug message, then trigger an architecture-specific breakpoint trap.
void debug_break(const char* reason);
