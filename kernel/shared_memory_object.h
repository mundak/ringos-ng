#pragma once

#include "kernel_object.h"

#include <stddef.h>
#include <stdint.h>

class shared_memory_object final : public kernel_object
{
public:
  shared_memory_object(uint32_t object_id, handle_t handle_value, uintptr_t user_address, size_t size);
  uintptr_t get_user_address() const;
  size_t get_size() const;

private:
  uintptr_t m_user_address;
  size_t m_size;
};
