#pragma once

#include <stdint.h>

// Initialize the PL011 UART at the QEMU virt base address.
void pl011_init();

// Write a single character to the PL011 UART.
void pl011_putc(char c);

// Write a null-terminated string to the PL011 UART.
void pl011_puts(const char* str);
