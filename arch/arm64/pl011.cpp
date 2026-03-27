#include "pl011.h"

#include <stdint.h>

namespace
{

  // QEMU virt PL011 UART base address.
  constexpr uintptr_t PL011_BASE = 0x09000000;

  // PL011 register offsets.
  constexpr uintptr_t UARTDR = 0x000;
  constexpr uintptr_t UARTFR = 0x018;
  constexpr uintptr_t UARTCR = 0x030;
  constexpr uintptr_t UARTIBRD = 0x024;
  constexpr uintptr_t UARTFBRD = 0x028;
  constexpr uintptr_t UARTLCR_H = 0x02C;

  // Flag register bits.
  constexpr uint32_t UARTFR_TXFF = (1U << 5);

  // Control register bits.
  constexpr uint32_t UARTCR_UARTEN = (1U << 0);
  constexpr uint32_t UARTCR_TXE = (1U << 8);

  // Line control bits.
  constexpr uint32_t UARTLCR_H_WLEN_8 = (3U << 5);
  constexpr uint32_t UARTLCR_H_FEN = (1U << 4);

  volatile uint32_t& reg(uintptr_t offset)
  {
    return *reinterpret_cast<volatile uint32_t*>(PL011_BASE + offset);
  }

}

void pl011_init()
{
  // Disable the UART while configuring.
  reg(UARTCR) = 0;

  // Set baud rate divisors — QEMU ignores these but we set them for
  // correctness on real hardware. Integer divisor = 1, fractional = 0
  // gives maximum speed on the emulated UART.
  reg(UARTIBRD) = 1;
  reg(UARTFBRD) = 0;

  // 8-bit word length, FIFO enabled.
  reg(UARTLCR_H) = UARTLCR_H_WLEN_8 | UARTLCR_H_FEN;

  // Enable UART and transmitter.
  reg(UARTCR) = UARTCR_UARTEN | UARTCR_TXE;
}

void pl011_putc(char c)
{
  // Wait until the transmit FIFO is not full.
  while (reg(UARTFR) & UARTFR_TXFF)
  {
  }

  reg(UARTDR) = static_cast<uint32_t>(c);
}

void pl011_puts(const char* str)
{
  while (*str != '\0')
  {
    pl011_putc(*str);
    ++str;
  }
}
