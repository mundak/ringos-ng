#pragma once

#include <ringos/rpc.h>
#include <ringos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ringos_console_protocol_version
{
  RINGOS_CONSOLE_PROTOCOL_VERSION_1 = 1,
  RINGOS_CONSOLE_PROTOCOL_VERSION_CURRENT = RINGOS_CONSOLE_PROTOCOL_VERSION_1,
};

enum ringos_console_operation
{
  RINGOS_CONSOLE_OPERATION_GET_INFO = 1,
  RINGOS_CONSOLE_OPERATION_WRITE = 2,
};

enum ringos_console_capability
{
  RINGOS_CONSOLE_CAPABILITY_WRITE = 1,
};

enum ringos_console_kind
{
  RINGOS_CONSOLE_KIND_UNKNOWN = 0,
  RINGOS_CONSOLE_KIND_SERIAL = 1,

  RINGOS_CONSOLE_KIND_VIRTUAL = 2,
};

typedef struct ringos_console_get_info_response
{
  uint32_t protocol_version;
  uint32_t console_kind;
  uint32_t capability_flags;
} ringos_console_get_info_response;

typedef struct ringos_console_write_request
{
  uintptr_t buffer_address;
  size_t buffer_size;
} ringos_console_write_request;

typedef struct ringos_console_write_response
{
  size_t bytes_written;
} ringos_console_write_response;

typedef struct ringos_console_device
{
  char endpoint_name[RINGOS_RPC_ENDPOINT_NAME_MAX_LENGTH + 1];
} ringos_console_device;

int32_t ringos_console_query_devices(ringos_console_device* devices, size_t device_capacity, size_t* out_device_count);
int32_t ringos_console_get_info(ringos_handle channel_handle, ringos_console_get_info_response* out_info);
int32_t ringos_console_write(
  ringos_handle channel_handle, const void* buffer, size_t buffer_size, size_t* out_bytes_written);

#ifdef __cplusplus
}
#endif
