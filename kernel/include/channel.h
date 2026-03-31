#pragma once

#include "kernel_object.h"
#include "user_abi_layouts.h"

class thread;

class channel final : public kernel_object
{
public:
  channel(uint32_t channel_id, handle_t handle_value, channel* peer);
  channel* get_peer() const;

private:
  friend class user_runtime;

  void set_peer(channel* peer);

  channel* m_peer;
  thread* m_waiting_thread;
  uintptr_t m_wait_request_address;
  thread* m_pending_client_thread;
  uintptr_t m_pending_client_response_address;
  user_rpc_request_layout m_pending_request;
};
