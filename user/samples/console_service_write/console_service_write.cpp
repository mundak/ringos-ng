#include "services/srv_terminal.h"

#include <kernelsdk/kernel_rpc.h>
#include <stddef.h>
#include <stdint.h>

int main()
{
  constexpr char message[] = "hello world through srv_terminal\n";
  constexpr size_t message_length = sizeof(message) - 1;
  static_assert(message_length <= TERMINAL_SERVICE_WRITE_MAX_BYTES, "message exceeds terminal service write limit");

  ringos_handle terminal_handle = RINGOS_HANDLE_INVALID;

  if (ringos_rpc_open(TERMINAL_SERVICE_RPC_ENDPOINT_NAME, &terminal_handle) != STATUS_OK)
  {
    return 1;
  }

  terminal_service_write_request write_request {};
  write_request.header.operation = TERMINAL_SERVICE_RPC_OPERATION_WRITE_BUFFER;
  write_request.length = message_length;
  for (size_t i = 0; i < message_length; ++i)
  {
    write_request.bytes[i] = static_cast<uint8_t>(message[i]);
  }

  if (ringos_rpc_call(terminal_handle, &write_request, sizeof(write_request)) != STATUS_OK)
  {
    static_cast<void>(ringos_rpc_close(terminal_handle));
    return 2;
  }

  return ringos_rpc_close(terminal_handle) == STATUS_OK ? 0 : 3;
}
