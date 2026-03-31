#pragma once

#include <ringos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RINGOS_CONSOLE_PROTOCOL_VERSION ((uint32_t) 1)

#define RINGOS_CONSOLE_OPERATION_GET_INFO ((uint64_t) 1)
#define RINGOS_CONSOLE_OPERATION_WRITE ((uint64_t) 2)

#define RINGOS_CONSOLE_CAPABILITY_WRITE ((uint32_t) 1)

#define RINGOS_CONSOLE_KIND_UNKNOWN ((uint32_t) 0)
#define RINGOS_CONSOLE_KIND_SERIAL ((uint32_t) 1)

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

