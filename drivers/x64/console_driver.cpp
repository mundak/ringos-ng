#include "console_driver.h"

#include <ringos/console.h>
#include <ringos/status.h>
#include <ringos/syscalls.h>

struct x64_console_driver::device_layout
{
  static constexpr uint32_t TX_CAPACITY = 256;

  volatile uint32_t tx_head;
  volatile uint32_t tx_tail;
  volatile char tx_buffer[TX_CAPACITY];
};

x64_console_driver::x64_console_driver()
  : m_device_layout(nullptr)
{
}

bool x64_console_driver::try_create(x64_console_driver& out_driver)
{
  uintptr_t device_memory_base_address = 0;
  size_t device_memory_size = 0;

  if (
    map_device_memory(device_memory_base_address, device_memory_size) != RINGOS_STATUS_OK
    || device_memory_size < sizeof(device_layout))
  {
    return false;
  }

  out_driver.m_device_layout = reinterpret_cast<volatile device_layout*>(device_memory_base_address);
  return true;
}

int32_t x64_console_driver::run() const
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

int32_t x64_console_driver::map_device_memory(uintptr_t& out_base_address, size_t& out_size)
{
  return ringos_syscall2(
    RINGOS_SYSCALL_DEVICE_MEMORY_MAP,
    reinterpret_cast<uintptr_t>(&out_base_address),
    reinterpret_cast<uintptr_t>(&out_size));
}

void x64_console_driver::handle_request(const ringos_rpc_request& request, ringos_rpc_response& response) const
{
  response.status = RINGOS_STATUS_NOT_SUPPORTED;

  switch (request.operation)
  {
  case RINGOS_CONSOLE_OPERATION_GET_INFO:
    response.status = RINGOS_STATUS_OK;
    response.value0 = RINGOS_CONSOLE_PROTOCOL_VERSION_CURRENT;
    response.value1 = RINGOS_CONSOLE_KIND_VIRTUAL;
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

int32_t x64_console_driver::write_console_bytes(const char* buffer, size_t length, size_t& out_bytes_written) const
{
  out_bytes_written = 0;

  while (out_bytes_written < length)
  {
    const uint32_t head = m_device_layout->tx_head % device_layout::TX_CAPACITY;
    const uint32_t tail = m_device_layout->tx_tail % device_layout::TX_CAPACITY;
    const uint32_t next_head = (head + 1U) % device_layout::TX_CAPACITY;

    if (next_head == tail)
    {
      break;
    }

    m_device_layout->tx_buffer[head] = buffer[out_bytes_written];
    m_device_layout->tx_head = next_head;
    ++out_bytes_written;
  }

  return out_bytes_written == length ? RINGOS_STATUS_OK : RINGOS_STATUS_WOULD_BLOCK;
}

int32_t main()
{
  x64_console_driver console_driver;

  if (!x64_console_driver::try_create(console_driver))
  {
    return 1;
  }

  return console_driver.run();
}
