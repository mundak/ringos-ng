#include "channel.h"

channel::channel(uint32_t channel_id, handle_t handle_value, channel* peer)
  : kernel_object(channel_id, handle_value)
  , m_peer(peer)
  , m_waiting_thread(nullptr)
  , m_wait_request_address(0)
  , m_pending_client_thread(nullptr)
  , m_pending_client_response_address(0)
  , m_pending_request {}
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
