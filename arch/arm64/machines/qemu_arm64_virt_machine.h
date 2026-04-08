#pragma once

#include "boot_info.h"
#include "machine.h"

bool try_initialize_qemu_arm64_virt_machine(const boot_info& info, machine_descriptor& out_machine);
