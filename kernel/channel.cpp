#include "channel.h"

channel* channel::peer() const
{
  return m_peer;
}

void channel::clear()
{
  clear_identity();
  m_peer = nullptr;
}

void channel::activate(
  uint32_t channel_id,
  uint64_t handle_value,
  channel* peer)
{
  clear();
  activate_identity(channel_id, handle_value);
  m_peer = peer;
}