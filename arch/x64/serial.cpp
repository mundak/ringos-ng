#include "serial.h"

#include "console.h"

#include <stdint.h>

namespace
{

  constexpr uint16_t COM1_PORT = 0x3F8;

  void outb(uint16_t port, uint8_t value)
  {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
  }

  uint8_t inb(uint16_t port)
  {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
  }

}

void serial_init()
{
  outb(COM1_PORT + 1, 0x00); // Disable interrupts
  outb(COM1_PORT + 3, 0x80); // Enable DLAB
  outb(COM1_PORT + 0, 0x03); // Baud rate divisor low byte (38400 baud)
  outb(COM1_PORT + 1, 0x00); // Baud rate divisor high byte
  outb(COM1_PORT + 3, 0x03); // 8 bits, no parity, one stop bit
  outb(COM1_PORT + 2, 0xC7); // Enable FIFO, clear, 14-byte threshold
  outb(COM1_PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

void console_write(const char* str)
{
  while (*str != '\0')
  {
    while ((inb(COM1_PORT + 5) & 0x20) == 0)
    {
    }
    outb(COM1_PORT, (uint8_t) *str);
    ++str;
  }
}
