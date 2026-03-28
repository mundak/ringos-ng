#pragma once

#include "kernel_object.h"

class channel final : public kernel_object
{
public:
  channel(uint32_t channel_id, handle_t handle_value, channel* peer);
  channel* get_peer() const;

private:
  friend class user_runtime;

  void set_peer(channel* peer);

  channel* m_peer;
};
