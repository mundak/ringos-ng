#include <ringos/console.h>
#include <ringos/debug.h>
#include <ringos/rpc.h>
#include <ringos/status.h>
#include <stdio.h>
#include <string.h>

namespace
{
  int write_console_message(const char* buffer, size_t length)
  {
    ringos_rpc_request request {};
    request.operation = RINGOS_CONSOLE_OPERATION_WRITE;
    request.argument0 = reinterpret_cast<uintptr_t>(buffer);
    request.argument1 = static_cast<uintptr_t>(length);

    ringos_rpc_response response {};
    const int32_t transport_status = ringos_rpc_call(&request, &response);

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
