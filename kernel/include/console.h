#pragma once

// Write a null-terminated string to the serial console.
// Arch-provided; must be callable after console init.
void console_write(const char* str);
