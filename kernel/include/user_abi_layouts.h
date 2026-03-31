#pragma once

#include <stdint.h>

struct user_rpc_request_layout
{
  uint64_t operation;
  uintptr_t argument0;
  uintptr_t argument1;
  uintptr_t argument2;
  uintptr_t argument3;
};

struct user_rpc_response_layout
{
  int32_t status;
  uint32_t reserved0;
  uintptr_t value0;
  uintptr_t value1;
  uintptr_t value2;
  uintptr_t value3;
};
