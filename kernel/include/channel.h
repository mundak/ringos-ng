#pragma once

#include "kernel_object.h"

class channel final : public kernel_object
{
public:
  channel* peer() const;

private:
  friend class user_runtime;

  void clear();
  void activate(
    uint32_t channel_id,
    uint64_t handle_value,
    channel* peer);

  channel* m_peer;
};