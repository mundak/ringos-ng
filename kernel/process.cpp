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

channel* process::get_assist_channel() const
{
  return m_assist_channel;
}

device_memory_object* process::get_assist_device_memory_object() const
{
  return m_assist_device_memory_object;
}

void process::set_assist_channel(channel* assist_channel)
{
  m_assist_channel = assist_channel;
}

void process::set_assist_device_memory_object(device_memory_object* assist_device_memory_object)
{
  m_assist_device_memory_object = assist_device_memory_object;
}
