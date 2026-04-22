#include "services/srv_terminal.h"

#include <kernelsdk/kernel_debug.h>
#include <kernelsdk/kernel_rpc.h>
#include <stddef.h>
#include <stdint.h>

namespace
{
  constexpr size_t TERMINAL_SERVICE_LINE_CAPACITY = 95;

  char g_terminal_line[TERMINAL_SERVICE_LINE_CAPACITY + 1] = {};
  size_t g_terminal_line_length = 0;

  int32_t terminal_flush_line()
  {
    if (g_terminal_line_length == 0)
    {
      return STATUS_OK;
    }

    g_terminal_line[g_terminal_line_length] = '\0';
    const int32_t status = ringos_debug_log(g_terminal_line);
    g_terminal_line_length = 0;
    return status;
  }

  int32_t terminal_write_character(uintptr_t character_value)
  {
    if (character_value > UINT8_MAX)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    const char character = static_cast<char>(character_value);

    if (character == '\0' || character == '\r')
    {
      return STATUS_OK;
    }

    if (character == '\n')
    {
      return terminal_flush_line();
    }

    if (g_terminal_line_length == TERMINAL_SERVICE_LINE_CAPACITY)
    {
      const int32_t flush_status = terminal_flush_line();

      if (flush_status != STATUS_OK)
      {
        return flush_status;
      }
    }

    g_terminal_line[g_terminal_line_length++] = character;
    return STATUS_OK;
  }

  int32_t terminal_handle_request(const ringos_rpc_request* request)
  {
    if (request == nullptr)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    switch (request->operation)
    {
    case TERMINAL_SERVICE_RPC_OPERATION_WRITE_CHARACTER:
    {
      return terminal_write_character(request->argument0);
    }

    default:
    {
      return STATUS_NOT_SUPPORTED;
    }
    }
  }
}

int main()
{
  const int32_t register_status = ringos_rpc_register(TERMINAL_SERVICE_RPC_ENDPOINT_NAME, terminal_handle_request);

  if (register_status != STATUS_OK)
  {
    return 1;
  }

  for (;;)
  {
  }
}
