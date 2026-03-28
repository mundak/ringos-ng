#include "channel.h"

channel::channel(uint32_t channel_id, handle_t handle_value, channel* peer)
  : kernel_object(channel_id, handle_value)
  , m_peer(peer)
{
}

channel* channel::get_peer() const
{
  return m_peer;
}

void channel::set_peer(channel* peer)
{
  m_peer = peer;
}
