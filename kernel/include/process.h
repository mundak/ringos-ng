#pragma once

#include "kernel_object.h"
#include "user_runtime_types.h"

class process final : public kernel_object
{
public:
  process(uint32_t process_id, handle_t handle_value, const address_space& address_space_info);
  const address_space& get_address_space_info() const;

private:
  address_space m_address_space;
};
