#pragma once

#include "kernel_object.h"

#include <stddef.h>
#include <stdint.h>

class device_memory_object final : public kernel_object
{
public:
  device_memory_object(uint32_t object_id, handle_t handle_value, uintptr_t user_address, uintptr_t host_address, size_t size);
  uintptr_t get_user_address() const;
  uintptr_t get_host_address() const;
  size_t get_size() const;

private:
  uintptr_t m_user_address;
  uintptr_t m_host_address;
  size_t m_size;
};
