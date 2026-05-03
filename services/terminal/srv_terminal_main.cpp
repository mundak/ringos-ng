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


  int32_t terminal_write_character(uint8_t character_value)
  {
    const char character = static_cast<char>(character_value);
    if (character == '\0' || character == '\r')
      return STATUS_OK;
    if (character == '\n')
      return terminal_flush_line();
    if (g_terminal_line_length == TERMINAL_SERVICE_LINE_CAPACITY)
    {
      int32_t flush_status = terminal_flush_line();
      if (flush_status != STATUS_OK)
        return flush_status;
    }
    g_terminal_line[g_terminal_line_length++] = character;
    return STATUS_OK;
  }

  int32_t terminal_write_buffer(const terminal_service_write_request* write_request)
  {
    if (write_request->length == 0 || write_request->length > TERMINAL_SERVICE_WRITE_MAX_BYTES)
    {
      return STATUS_INVALID_ARGUMENT;
    }
    for (uint64_t i = 0; i < write_request->length; ++i)
    {
      int32_t status = terminal_write_character(write_request->bytes[i]);
      if (status != STATUS_OK)
      {
        return status;
      }
    }
    return STATUS_OK;
  }

  int32_t terminal_handle_request(const void* request, size_t request_size)
  {
    if (request == nullptr || request_size < sizeof(terminal_service_request_header))
    {
      return STATUS_INVALID_ARGUMENT;
    }

    const auto* header = static_cast<const terminal_service_request_header*>(request);

    switch (header->operation)
    {
    case TERMINAL_SERVICE_RPC_OPERATION_WRITE_BUFFER:
      if (request_size != sizeof(terminal_service_write_request))
      {
        return STATUS_INVALID_ARGUMENT;
      }
      return terminal_write_buffer(static_cast<const terminal_service_write_request*>(request));
    default:
      return STATUS_NOT_SUPPORTED;
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
