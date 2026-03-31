#pragma once

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
};

typedef struct ringos_console_get_info_response
{
  uint32_t protocol_version;
  uint32_t console_kind;
  uint32_t capability_flags;
  uint32_t reserved0;
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

#ifdef __cplusplus
}
#endif
