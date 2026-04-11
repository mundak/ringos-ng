#pragma once

// Kernel interface.

enum ringos_console_device_type
{
  RINGOS_DEVICE_TYPE_CONSOLE = 1,
};

enum ringos_console_protocol_version
{
  RINGOS_DEVICE_CONSOLE_PROTOCOL_VERSION_1 = 1,
  RINGOS_DEVICE_CONSOLE_PROTOCOL_VERSION_CURRENT = RINGOS_DEVICE_CONSOLE_PROTOCOL_VERSION_1,
};

// Driver interface:
// ringos_rpc_status ringos_rpc_call(handle, RINGOS_DEVICE_CONSOLE_OPERATION_GET_INFO, ringos_console_info* out)
// ringos_rpc_status ringos_rpc_call(handle, RINGOS_DEVICE_CONSOLE_OPERATION_WRITE, const char* buffer, size_t
// buffer_size, size_t* bytes_written)

enum ringos_console_required_rpcs
{
  RINGOS_DEVICE_CONSOLE_OPERATION_GET_INFO = 1,
  RINGOS_DEVICE_CONSOLE_OPERATION_WRITE = 2,
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

typedef struct ringos_console_info
{
  uint32_t protocol_version;
  uint32_t console_kind;
  uint32_t capability_flags;
} ringos_console_info;
