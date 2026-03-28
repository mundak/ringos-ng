#include "process.h"

process::process(uint32_t process_id, handle_t handle_value, const address_space& address_space_info)
  : kernel_object(process_id, handle_value)
  , m_address_space(address_space_info)
{
}

const address_space& process::get_address_space_info() const
{
  return m_address_space;
}
