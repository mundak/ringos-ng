#pragma once

#include "kernel_object.h"
#include "user_runtime_types.h"

class process final : public kernel_object
{
public:
  const address_space& address_space_info() const;

private:
  friend class user_runtime;

  void clear();
  void activate(
    uint32_t process_id,
    uint64_t handle_value,
    const address_space& address_space_info);

  address_space m_address_space;
};