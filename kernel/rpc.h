#pragma once

#include "kernel_object.h"
#include "kernel_object_pool.h"
#include "thread.h"

#include <ringos/rpc.h>

class process;
class user_runtime;

class rpc_endpoint final
{
public:
  bool is_in_use() const;
  bool is_busy() const;
  bool matches_name(const char* name) const;
  process* get_owner_process() const;
  thread* get_owner_thread() const;
  uintptr_t get_callback_address() const;
  uintptr_t get_completion_address() const;

  void clear();
  void initialize(
    process& owner_process,
    thread& owner_thread,
    const char* name,
    uintptr_t callback_address,
    uintptr_t completion_address);
  void set_busy(bool is_busy);

private:
  static constexpr size_t NAME_CAPACITY = RINGOS_RPC_ENDPOINT_NAME_MAX_LENGTH + 1;

  bool m_is_in_use = false;
  bool m_is_busy = false;
  char m_name[NAME_CAPACITY] {};
  process* m_owner_process = nullptr;
  thread* m_owner_thread = nullptr;
  uintptr_t m_callback_address = 0;
  uintptr_t m_completion_address = 0;
};

class rpc_channel final : public kernel_object
{
public:
  rpc_channel(uint32_t channel_id, handle_t handle_value, process& client_process, rpc_endpoint& endpoint);

  process* get_client_process() const;
  rpc_endpoint* get_endpoint() const;
  thread* get_pending_client_thread() const;
  thread* get_pending_server_thread() const;
  bool is_busy() const;
  bool is_owned_by(const process& process_context) const;

  void begin_call(thread& client_thread, thread& server_thread);
  void cancel_call();
  void finish_call();

private:
  process* m_client_process;
  rpc_endpoint* m_endpoint;
  thread* m_pending_client_thread = nullptr;
  thread* m_pending_server_thread = nullptr;
  user_thread_state m_saved_server_state = USER_THREAD_STATE_EMPTY;
  thread_context m_saved_server_context {};
  uintptr_t m_saved_server_registers[USER_THREAD_ARCH_PRESERVED_REGISTER_COUNT] {};
  uint64_t m_saved_server_simd_qwords[USER_THREAD_ARCH_PRESERVED_SIMD_QWORD_COUNT] {};
};

class rpc_runtime final
{
public:
  void reset(handle_t& next_handle_value);
  void handle_thread_exit(thread& active_thread, int32_t failure_status);
  int32_t dispatch_syscall(user_runtime& runtime, thread& active_thread, const user_syscall_context& syscall_context);

private:
  rpc_channel* find_channel_by_handle(handle_t handle_value);
  rpc_endpoint* find_endpoint_by_name(const char* endpoint_name);
  rpc_endpoint* find_free_endpoint();
  rpc_channel* find_pending_channel_by_server_thread(thread* server_thread);
  void clear_endpoints_owned_by(thread& owner_thread);
  int32_t complete_call(user_runtime& runtime, thread& server_thread, int32_t callback_status);
  int32_t fail_pending_call(thread& server_thread, int32_t failure_status);
  int32_t prepare_server_thread(
    user_runtime& runtime, const rpc_endpoint& endpoint, thread& server_thread, const ringos_rpc_request& request);

  rpc_endpoint m_endpoints[USER_RUNTIME_MAX_RPC_ENDPOINTS] {};
  kernel_object_pool<rpc_channel, USER_RUNTIME_MAX_RPC_CHANNELS> m_channels;
};
