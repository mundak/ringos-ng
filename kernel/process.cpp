#include "process.h"

#include "memory.h"

const address_space& process::address_space_info() const
{
  return m_address_space;
}

void process::clear()
{
  clear_identity();
  memset(&m_address_space, 0, sizeof(m_address_space));
}

void process::activate(
  uint32_t process_id,
  uint64_t handle_value,
  const address_space& address_space_info)
{
  clear();
  activate_identity(process_id, handle_value);
  m_address_space = address_space_info;
}