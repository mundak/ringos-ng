#pragma once

#include "device_memory_type.h"
#include "kernel_object.h"

#include <stddef.h>
#include <stdint.h>

class device_memory_object final : public kernel_object
{
public:
  device_memory_object(
    uint32_t object_id,
    handle_t handle_value,
    device_memory_type type,
    uintptr_t user_address,
    uintptr_t host_address,
    size_t size);
  device_memory_type get_type() const;
  uintptr_t get_user_address() const;
  uintptr_t get_host_address() const;
  size_t get_size() const;

private:
  device_memory_type m_type;
  uintptr_t m_user_address;
  uintptr_t m_host_address;
  size_t m_size;
};
