#include "shared_memory_object.h"

uintptr_t shared_memory_object::user_address() const
{
  return m_user_address;
}

size_t shared_memory_object::size() const
{
  return m_size;
}

void shared_memory_object::clear()
{
  clear_identity();
  m_user_address = 0;
  m_size = 0;
}

void shared_memory_object::activate(
  uint32_t object_id,
  uint64_t handle_value,
  uintptr_t user_address,
  size_t size)
{
  clear();
  activate_identity(object_id, handle_value);
  m_user_address = user_address;
  m_size = size;
}