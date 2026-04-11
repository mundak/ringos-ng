#pragma once

#include <stdint.h>

enum device_memory_type : uint32_t
{
  DEVICE_MEMORY_TYPE_NONE = 0,
  DEVICE_MEMORY_TYPE_VIRTUAL_CONSOLE_BUFFER = 1,
  DEVICE_MEMORY_TYPE_MMIO = 2,
};
