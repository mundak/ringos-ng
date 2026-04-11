#pragma once

#include "boot_info.h"

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

struct machine_descriptor
{
  uint32_t arch_id;
  uint32_t machine_kind;
  char name[MACHINE_NAME_MAX_LENGTH];
};

void initialize_machine(const boot_info& info);
const machine_descriptor& get_machine();
