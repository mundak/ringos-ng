#include "shared_memory_object.h"

shared_memory_object::shared_memory_object(
  uint32_t object_id, handle_t handle_value, uintptr_t user_address, size_t size)
  : kernel_object(object_id, handle_value)
  , m_user_address(user_address)
  , m_size(size)
{
}

uintptr_t shared_memory_object::get_user_address() const
{
  return m_user_address;
}

size_t shared_memory_object::get_size() const
{
  return m_size;
}
