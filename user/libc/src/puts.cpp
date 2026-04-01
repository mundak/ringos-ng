#include <ringos/console.h>
#include <ringos/debug.h>
#include <ringos/rpc.h>
#include <ringos/status.h>
#include <stdio.h>
#include <string.h>

namespace
{
  ringos_handle get_default_console_channel()
  {
    static ringos_handle channel_handle = RINGOS_HANDLE_INVALID;

    if (channel_handle != RINGOS_HANDLE_INVALID)
    {
      return channel_handle;
    }

    ringos_console_device devices[1] {};
    size_t device_count = 0;
    const int32_t query_status = ringos_console_query_devices(devices, 1, &device_count);

    if ((query_status != RINGOS_STATUS_OK && query_status != RINGOS_STATUS_BUFFER_TOO_SMALL) || device_count == 0)
    {
      return RINGOS_HANDLE_INVALID;
    }

    if (ringos_rpc_open(devices[0].endpoint_name, &channel_handle) != RINGOS_STATUS_OK)
    {
      channel_handle = RINGOS_HANDLE_INVALID;
    }

    return channel_handle;
  }

  int write_console_message(const char* buffer, size_t length)
  {
    const ringos_handle channel_handle = get_default_console_channel();

    if (channel_handle == RINGOS_HANDLE_INVALID)
    {
      return EOF;
    }

    ringos_rpc_request request {};
    request.operation = RINGOS_CONSOLE_OPERATION_WRITE;
    request.argument0 = reinterpret_cast<uintptr_t>(buffer);
    request.argument1 = static_cast<uintptr_t>(length);

    ringos_rpc_response response {};
    const int32_t transport_status = ringos_rpc_call(channel_handle, &request, &response);

    if (transport_status != RINGOS_STATUS_OK || response.status != RINGOS_STATUS_OK)
    {
      return EOF;
    }

    return static_cast<size_t>(response.value0) == length ? 0 : EOF;
  }
}

int puts(const char* string)
{
  if (string == nullptr)
  {
    return EOF;
  }

  const size_t string_length = strlen(string);

  if (write_console_message(string, string_length) == EOF)
  {
    return ringos_debug_log(string);
  }

  return write_console_message("\n", 1);
}
