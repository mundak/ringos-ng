#pragma once

#include "kernel_object.h"
#include "user_runtime_types.h"

class channel;
class device_memory_object;

class process final : public kernel_object
{
public:
  process(uint32_t process_id, handle_t handle_value, const address_space& address_space_info);
  const address_space& get_address_space_info() const;
  channel* get_assist_channel() const;
  device_memory_object* get_assist_device_memory_object() const;

  void set_assist_channel(channel* assist_channel);
  void set_assist_device_memory_object(device_memory_object* assist_device_memory_object);

private:
  address_space m_address_space;
  channel* m_assist_channel = nullptr;
  device_memory_object* m_assist_device_memory_object = nullptr;
};
