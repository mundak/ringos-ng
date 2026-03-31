#include "device_memory_object.h"

device_memory_object::device_memory_object(
  uint32_t object_id, handle_t handle_value, uintptr_t user_address, uintptr_t host_address, size_t size)
  : kernel_object(object_id, handle_value)
  , m_user_address(user_address)
  , m_host_address(host_address)
  , m_size(size)
{
}

uintptr_t device_memory_object::get_user_address() const
{
  return m_user_address;
}

uintptr_t device_memory_object::get_host_address() const
{
  return m_host_address;
}

size_t device_memory_object::get_size() const
{
  return m_size;
}
