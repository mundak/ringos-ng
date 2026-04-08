#include "ringos/console.h"

#include <ringos/rpc.h>
#include <ringos/status.h>
#include <ringos/syscalls.h>

int32_t ringos_console_query_devices(ringos_console_device* devices, size_t device_capacity, size_t* out_device_count)
{
  if (out_device_count == nullptr || (devices == nullptr && device_capacity != 0))
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall3(
    RINGOS_SYSCALL_CONSOLE_QUERY,
    reinterpret_cast<uintptr_t>(devices),
    static_cast<uintptr_t>(device_capacity),
    reinterpret_cast<uintptr_t>(out_device_count));
}

int32_t ringos_console_get_info(ringos_handle channel_handle, ringos_console_get_info_response* out_info)
{
  if (channel_handle == RINGOS_HANDLE_INVALID || out_info == nullptr)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  ringos_rpc_request request {};
  request.operation = RINGOS_CONSOLE_OPERATION_GET_INFO;

  ringos_rpc_response response {};
  const ringos_rpc_status transport_status = ringos_rpc_call(channel_handle, &request, &response);

  if (transport_status != RINGOS_STATUS_OK)
  {
    return transport_status;
  }

  if (response.status != RINGOS_STATUS_OK)
  {
    return response.status;
  }

  out_info->protocol_version = static_cast<uint32_t>(response.value0);
  out_info->console_kind = static_cast<uint32_t>(response.value1);
  out_info->capability_flags = static_cast<uint32_t>(response.value2);
  return RINGOS_STATUS_OK;
}

int32_t ringos_console_write(
  ringos_handle channel_handle, const void* buffer, size_t buffer_size, size_t* out_bytes_written)
{
  if (channel_handle == RINGOS_HANDLE_INVALID || (buffer == nullptr && buffer_size != 0))
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  ringos_rpc_request request {};
  request.operation = RINGOS_CONSOLE_OPERATION_WRITE;
  request.argument0 = reinterpret_cast<uintptr_t>(buffer);
  request.argument1 = static_cast<uintptr_t>(buffer_size);

  ringos_rpc_response response {};
  const ringos_rpc_status transport_status = ringos_rpc_call(channel_handle, &request, &response);

  if (transport_status != RINGOS_STATUS_OK)
  {
    return transport_status;
  }

  if (out_bytes_written != nullptr)
  {
    *out_bytes_written = static_cast<size_t>(response.value0);
  }

  return response.status;
}
