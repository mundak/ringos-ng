#include "kernel_object.h"

kernel_object::kernel_object(uint32_t object_id, handle_t handle_value)
{
  m_id = object_id;
  m_handle_value = handle_value;
  m_process_reference_count = 0;
}

kernel_object::~kernel_object()
{
}

uint32_t kernel_object::get_id() const
{
  return m_id;
}

handle_t kernel_object::get_handle() const
{
  return m_handle_value;
}

uint32_t kernel_object::get_process_reference_count() const
{
  return m_process_reference_count;
}

void kernel_object::acquire_process_reference()
{
  ++m_process_reference_count;
}

