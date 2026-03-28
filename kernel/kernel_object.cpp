#include "kernel_object.h"

bool kernel_object::is_in_use() const
{
  return m_in_use;
}

uint32_t kernel_object::id() const
{
  return m_id;
}

uint64_t kernel_object::handle_value() const
{
  return m_handle_value;
}

uint32_t kernel_object::process_reference_count() const
{
  return m_process_reference_count;
}

void kernel_object::clear_identity()
{
  m_in_use = false;
  m_id = 0;
  m_handle_value = 0;
  m_process_reference_count = 0;
}

void kernel_object::activate_identity(
  uint32_t object_id,
  uint64_t handle_value)
{
  m_in_use = true;
  m_id = object_id;
  m_handle_value = handle_value;
}

void kernel_object::acquire_process_reference()
{
  ++m_process_reference_count;
}