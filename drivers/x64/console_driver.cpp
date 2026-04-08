#include <ringos/console.h>
#include <ringos/rpc.h>
#include <ringos/status.h>
#include <ringos/syscalls.h>

#include <stddef.h>
#include <stdint.h>

namespace
{
  constexpr uint32_t X64_CONSOLE_DEVICE_TX_CAPACITY = 256;

  struct x64_console_device_layout
  {
    volatile uint32_t tx_head;
    volatile uint32_t tx_tail;
    volatile char tx_buffer[X64_CONSOLE_DEVICE_TX_CAPACITY];
  };

  int32_t map_device_memory(uintptr_t* out_base_address, size_t* out_size)
  {
    if (out_base_address == nullptr || out_size == nullptr)
    {
      return RINGOS_STATUS_INVALID_ARGUMENT;
    }

    return ringos_syscall2(
      RINGOS_SYSCALL_DEVICE_MEMORY_MAP,
      reinterpret_cast<uintptr_t>(out_base_address),
      reinterpret_cast<uintptr_t>(out_size));
  }

  int32_t write_console_bytes(
    volatile x64_console_device_layout* device_layout, const char* buffer, size_t length, size_t* out_bytes_written)
  {
    size_t bytes_written = 0;

    while (bytes_written < length)
    {
      const uint32_t head = device_layout->tx_head % X64_CONSOLE_DEVICE_TX_CAPACITY;
      const uint32_t tail = device_layout->tx_tail % X64_CONSOLE_DEVICE_TX_CAPACITY;
      const uint32_t next_head = (head + 1U) % X64_CONSOLE_DEVICE_TX_CAPACITY;

      if (next_head == tail)
      {
        break;
      }

      device_layout->tx_buffer[head] = buffer[bytes_written];
      device_layout->tx_head = next_head;
      ++bytes_written;
    }

    if (out_bytes_written != nullptr)
    {
      *out_bytes_written = bytes_written;
    }

    return bytes_written == length ? RINGOS_STATUS_OK : RINGOS_STATUS_WOULD_BLOCK;
  }
}

int main()
{
  uintptr_t device_memory_base_address = 0;
  size_t device_memory_size = 0;

  if (
    map_device_memory(&device_memory_base_address, &device_memory_size) != RINGOS_STATUS_OK
    || device_memory_size < sizeof(x64_console_device_layout))
  {
    return 1;
  }

  volatile x64_console_device_layout* const device_layout
    = reinterpret_cast<volatile x64_console_device_layout*>(device_memory_base_address);

  for (;;)
  {
    ringos_rpc_request request {};

    if (ringos_rpc_wait(&request) != RINGOS_STATUS_OK)
    {
      return 1;
    }

    ringos_rpc_response response {};
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
        device_layout,
        reinterpret_cast<const char*>(request.argument0),
        static_cast<size_t>(request.argument1),
        &bytes_written);
      response.value0 = static_cast<uintptr_t>(bytes_written);
      break;
    }

    default:
      break;
    }

    if (ringos_rpc_reply(&response) != RINGOS_STATUS_OK)
    {
      return 1;
    }
  }
}
