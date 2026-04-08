#include "qemu_arm64_virt_console.h"

#include <ringos/console.h>
#include <ringos/status.h>
#include <ringos/syscalls.h>

namespace
{
  constexpr uintptr_t PL011_DATA_REGISTER_OFFSET = 0x000;
  constexpr uintptr_t PL011_FLAG_REGISTER_OFFSET = 0x018;
  constexpr uintptr_t PL011_MINIMUM_REGISTER_WINDOW_SIZE = PL011_FLAG_REGISTER_OFFSET + sizeof(uint32_t);
  constexpr uint32_t PL011_FLAG_TRANSMIT_FIFO_FULL = 1U << 5;
  constexpr uint32_t PL011_TRANSMIT_FIFO_WAIT_RETRY_LIMIT = 1U << 20;

  void yield_processor()
  {
    asm volatile("yield" : : : "memory");
  }
}

qemu_arm64_virt_console::qemu_arm64_virt_console()
  : m_mmio_base(nullptr)
{
}

bool qemu_arm64_virt_console::try_create(qemu_arm64_virt_console& out_console)
{
  uintptr_t device_memory_base_address = 0;
  size_t device_memory_size = 0;

  if (
    map_device_memory(device_memory_base_address, device_memory_size) != RINGOS_STATUS_OK
    || device_memory_size < PL011_MINIMUM_REGISTER_WINDOW_SIZE)
  {
    return false;
  }

  out_console.m_mmio_base = reinterpret_cast<volatile uint8_t*>(device_memory_base_address);
  return true;
}

int32_t qemu_arm64_virt_console::run() const
{
  for (;;)
  {
    ringos_rpc_request request {};

    if (ringos_rpc_wait(&request) != RINGOS_STATUS_OK)
    {
      return 1;
    }

    ringos_rpc_response response {};
    handle_request(request, response);

    if (ringos_rpc_reply(&response) != RINGOS_STATUS_OK)
    {
      return 1;
    }
  }
}

int32_t qemu_arm64_virt_console::map_device_memory(uintptr_t& out_base_address, size_t& out_size)
{
  return ringos_syscall2(
    RINGOS_SYSCALL_DEVICE_MEMORY_MAP,
    reinterpret_cast<uintptr_t>(&out_base_address),
    reinterpret_cast<uintptr_t>(&out_size));
}

void qemu_arm64_virt_console::handle_request(const ringos_rpc_request& request, ringos_rpc_response& response) const
{
  response.status = RINGOS_STATUS_NOT_SUPPORTED;

  switch (request.operation)
  {
  case RINGOS_CONSOLE_OPERATION_GET_INFO:
    response.status = RINGOS_STATUS_OK;
    response.value0 = RINGOS_CONSOLE_PROTOCOL_VERSION_CURRENT;
    response.value1 = RINGOS_CONSOLE_KIND_SERIAL;
    response.value2 = RINGOS_CONSOLE_CAPABILITY_WRITE;
    break;

  case RINGOS_CONSOLE_OPERATION_WRITE:
  {
    size_t bytes_written = 0;
    response.status = write_console_bytes(
      reinterpret_cast<const char*>(request.argument0), static_cast<size_t>(request.argument1), bytes_written);
    response.value0 = static_cast<uintptr_t>(bytes_written);
    break;
  }

  default:
    break;
  }
}

bool qemu_arm64_virt_console::is_transmit_fifo_full() const
{
  const volatile uint32_t* const flag_register
    = reinterpret_cast<volatile uint32_t*>(m_mmio_base + PL011_FLAG_REGISTER_OFFSET);
  return (*flag_register & PL011_FLAG_TRANSMIT_FIFO_FULL) != 0;
}

void qemu_arm64_virt_console::write_transmit_byte(char value) const
{
  volatile uint32_t* const data_register
    = reinterpret_cast<volatile uint32_t*>(m_mmio_base + PL011_DATA_REGISTER_OFFSET);
  *data_register = static_cast<uint32_t>(static_cast<uint8_t>(value));
}

int32_t qemu_arm64_virt_console::write_console_bytes(const char* buffer, size_t length, size_t& out_bytes_written) const
{
  out_bytes_written = 0;
  uint32_t remaining_retries = PL011_TRANSMIT_FIFO_WAIT_RETRY_LIMIT;

  while (out_bytes_written < length)
  {
    while (is_transmit_fifo_full())
    {
      if (remaining_retries-- == 0)
      {
        return RINGOS_STATUS_WOULD_BLOCK;
      }

      yield_processor();
    }

    write_transmit_byte(buffer[out_bytes_written]);
    ++out_bytes_written;
  }

  return out_bytes_written == length ? RINGOS_STATUS_OK : RINGOS_STATUS_WOULD_BLOCK;
}

int32_t main()
{
  qemu_arm64_virt_console console;

  if (!qemu_arm64_virt_console::try_create(console))
  {
    return 1;
  }

  return console.run();
}
