#pragma once

#include "boot_info.h"
#include "machine.h"

bool arch_initialize_machine(const boot_info& info, machine_descriptor& out_machine);
