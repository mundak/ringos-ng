#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel_object.h"

class shared_memory_object final : public kernel_object
{
public:
  uintptr_t user_address() const;
  size_t size() const;

private:
  friend class user_runtime;

  void clear();
  void activate(
    uint32_t object_id,
    uint64_t handle_value,
    uintptr_t user_address,
    size_t size);

  uintptr_t m_user_address;
  size_t m_size;
};