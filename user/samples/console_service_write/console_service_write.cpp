#include "services/srv_terminal.h"

#include <kernelsdk/kernel_rpc.h>
#include <stddef.h>
#include <stdint.h>

int main()
{
  constexpr char message[] = "hello world through srv_terminal\n";

  ringos_handle terminal_handle = RINGOS_HANDLE_INVALID;

  if (ringos_rpc_open(TERMINAL_SERVICE_RPC_ENDPOINT_NAME, &terminal_handle) != STATUS_OK)
  {
    return 1;
  }

  for (size_t index = 0; message[index] != '\0'; ++index)
  {
    const ringos_rpc_request request {
      TERMINAL_SERVICE_RPC_OPERATION_WRITE_CHARACTER,
      static_cast<uintptr_t>(static_cast<uint8_t>(message[index])),
      0,
      0,
      0,
    };

    if (ringos_rpc_call(terminal_handle, &request) != STATUS_OK)
    {
      static_cast<void>(ringos_rpc_close(terminal_handle));
      return 2;
    }
  }

  return ringos_rpc_close(terminal_handle) == STATUS_OK ? 0 : 3;
}
