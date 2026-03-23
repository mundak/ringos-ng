#pragma once

#include <stdint.h>

// Write a null-terminated string to the serial console.
void kprint(const char* str);

// Write an unsigned 64-bit integer in decimal notation to the serial console.
void kprint_uint(uint64_t value);

// Write an unsigned 64-bit integer in hexadecimal notation to the serial
// console, prefixed with "0x" and zero-padded to 16 digits.
void kprint_hex(uint64_t value);
