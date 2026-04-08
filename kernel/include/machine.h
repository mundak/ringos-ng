#pragma once

#include "boot_info.h"
#include "device_memory_type.h"

#include <stddef.h>
#include <stdint.h>

static constexpr size_t MACHINE_NAME_MAX_LENGTH = 32;
static constexpr size_t MACHINE_DEVICE_STRING_MAX_LENGTH = 96;

enum machine_kind : uint32_t
{
  MACHINE_KIND_UNKNOWN = 0,
  MACHINE_KIND_X64_GENERIC = 1,
  MACHINE_KIND_QEMU_ARM64_VIRT = 2,
};

enum machine_console_register_model : uint32_t
{
  MACHINE_CONSOLE_REGISTER_MODEL_NONE = 0,
  MACHINE_CONSOLE_REGISTER_MODEL_VIRTUAL_BUFFER = 1,
  MACHINE_CONSOLE_REGISTER_MODEL_PL011 = 2,
};

struct machine_console_descriptor
{
  uint32_t register_model;
  device_memory_type device_memory_type;
  uintptr_t mmio_physical_address;
  size_t mmio_size;
  char device_tree_path[MACHINE_DEVICE_STRING_MAX_LENGTH];
  char compatible[MACHINE_DEVICE_STRING_MAX_LENGTH];
};

struct machine_descriptor
{
  uint32_t arch_id;
  uint32_t machine_kind;
  char name[MACHINE_NAME_MAX_LENGTH];
  machine_console_descriptor console;
};

void initialize_machine(const boot_info& info);
const machine_descriptor& get_machine();
