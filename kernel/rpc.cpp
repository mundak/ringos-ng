#include "rpc.h"

#include "klibc/memory.h"
#include "klibc/string.h"
#include "process.h"
#include "user_runtime.h"

namespace
{
  constexpr size_t RPC_ENDPOINT_NAME_BUFFER_SIZE = RINGOS_RPC_ENDPOINT_NAME_MAX_LENGTH + 1;

  bool strings_equal(const char* left, const char* right)
  {
    if (left == nullptr || right == nullptr)
    {
      return false;
    }

    for (size_t index = 0;; ++index)
    {
      if (left[index] != right[index])
      {
        return false;
      }

      if (left[index] == '\0')
      {
        return true;
      }
    }
  }

  uintptr_t align_down(uintptr_t value, uintptr_t alignment)
  {
    if (alignment == 0)
    {
      return value;
    }

    return value & ~static_cast<uintptr_t>(alignment - 1);
  }
}

bool rpc_endpoint::is_in_use() const
{
  return m_is_in_use;
}

bool rpc_endpoint::is_busy() const
{
  return m_is_busy;
}

bool rpc_endpoint::matches_name(const char* name) const
{
  return m_is_in_use && strings_equal(m_name, name);
}

process* rpc_endpoint::get_owner_process() const
{
  return m_owner_process;
}

thread* rpc_endpoint::get_owner_thread() const
{
  return m_owner_thread;
}

uintptr_t rpc_endpoint::get_callback_address() const
{
  return m_callback_address;
}

uintptr_t rpc_endpoint::get_completion_address() const
{
  return m_completion_address;
}

void rpc_endpoint::clear()
{
  m_is_in_use = false;
  m_is_busy = false;
  memset(m_name, 0, sizeof(m_name));
  m_owner_process = nullptr;
  m_owner_thread = nullptr;
  m_callback_address = 0;
  m_completion_address = 0;
}

void rpc_endpoint::initialize(
  process& owner_process,
  thread& owner_thread,
  const char* name,
  uintptr_t callback_address,
  uintptr_t completion_address)
{
  clear();
  m_is_in_use = true;
  copy_string(m_name, sizeof(m_name), name);
  m_owner_process = &owner_process;
  m_owner_thread = &owner_thread;
  m_callback_address = callback_address;
  m_completion_address = completion_address;
}

void rpc_endpoint::set_busy(bool is_busy)
{
  m_is_busy = is_busy;
}

rpc_channel::rpc_channel(uint32_t channel_id, handle_t handle_value, process& client_process, rpc_endpoint& endpoint)
  : kernel_object(channel_id, handle_value)
  , m_client_process(&client_process)
  , m_endpoint(&endpoint)
{
}

process* rpc_channel::get_client_process() const
{
  return m_client_process;
}

rpc_endpoint* rpc_channel::get_endpoint() const
{
  return m_endpoint;
}

thread* rpc_channel::get_pending_client_thread() const
{
  return m_pending_client_thread;
}

thread* rpc_channel::get_pending_server_thread() const
{
  return m_pending_server_thread;
}

bool rpc_channel::is_busy() const
{
  return m_pending_client_thread != nullptr || m_pending_server_thread != nullptr;
}

bool rpc_channel::is_owned_by(const process& process_context) const
{
  return m_client_process == &process_context;
}

void rpc_channel::begin_call(thread& client_thread, thread& server_thread)
{
  m_pending_client_thread = &client_thread;
  m_pending_server_thread = &server_thread;
  m_saved_server_state = server_thread.get_state();
  m_saved_server_context = server_thread.get_user_context();
  memcpy(m_saved_server_registers, server_thread.get_arch_preserved_registers(), sizeof(m_saved_server_registers));
  memcpy(
    m_saved_server_simd_qwords, server_thread.get_arch_preserved_simd_qwords(), sizeof(m_saved_server_simd_qwords));

  if (m_endpoint != nullptr)
  {
    m_endpoint->set_busy(true);
  }
}

void rpc_channel::cancel_call()
{
  if (m_endpoint != nullptr)
  {
    m_endpoint->set_busy(false);
  }

  m_pending_client_thread = nullptr;
  m_pending_server_thread = nullptr;
  m_saved_server_state = USER_THREAD_STATE_EMPTY;
  m_saved_server_context = {};
  memset(m_saved_server_registers, 0, sizeof(m_saved_server_registers));
  memset(m_saved_server_simd_qwords, 0, sizeof(m_saved_server_simd_qwords));
}

void rpc_channel::finish_call()
{
  if (m_pending_server_thread != nullptr)
  {
    m_pending_server_thread->set_user_context(m_saved_server_context);
    m_pending_server_thread->set_state(m_saved_server_state);
    memcpy(
      m_pending_server_thread->get_arch_preserved_registers(),
      m_saved_server_registers,
      sizeof(m_saved_server_registers));
    memcpy(
      m_pending_server_thread->get_arch_preserved_simd_qwords(),
      m_saved_server_simd_qwords,
      sizeof(m_saved_server_simd_qwords));
  }

  cancel_call();
}

void rpc_runtime::reset(handle_t& next_handle_value)
{
  m_channels.reset(next_handle_value);

  for (uint32_t index = 0; index < USER_RUNTIME_MAX_RPC_ENDPOINTS; ++index)
  {
    m_endpoints[index].clear();
  }
}

void rpc_runtime::handle_thread_exit(thread& active_thread, int32_t failure_status)
{
  (void) fail_pending_call(active_thread, failure_status);
  clear_endpoints_owned_by(active_thread);
}

int32_t rpc_runtime::dispatch_syscall(
  user_runtime& runtime, thread& active_thread, const user_syscall_context& syscall_context)
{
  process* owner_process = active_thread.get_process_context();

  if (owner_process == nullptr)
  {
    return STATUS_BAD_STATE;
  }

  switch (syscall_context.syscall_number)
  {
  case SYSCALL_RPC_REGISTER:
  {
    if (syscall_context.argument0 == 0 || syscall_context.argument1 == 0 || syscall_context.argument2 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    if (
      !runtime.validate_user_range(*owner_process, static_cast<uintptr_t>(syscall_context.argument1), 1)
      || !runtime.validate_user_range(*owner_process, static_cast<uintptr_t>(syscall_context.argument2), 1))
    {
      return STATUS_FAULT;
    }

    char endpoint_name[RPC_ENDPOINT_NAME_BUFFER_SIZE];
    memset(endpoint_name, 0, sizeof(endpoint_name));
    const int32_t name_status = runtime.copy_user_string(
      active_thread, static_cast<uintptr_t>(syscall_context.argument0), endpoint_name, sizeof(endpoint_name));

    if (name_status != STATUS_OK)
    {
      return name_status;
    }

    if (find_endpoint_by_name(endpoint_name) != nullptr)
    {
      return STATUS_BAD_STATE;
    }

    rpc_endpoint* endpoint = find_free_endpoint();

    if (endpoint == nullptr)
    {
      return STATUS_NO_MEMORY;
    }

    endpoint->initialize(
      *owner_process,
      active_thread,
      endpoint_name,
      static_cast<uintptr_t>(syscall_context.argument1),
      static_cast<uintptr_t>(syscall_context.argument2));

    thread* next_thread = runtime.find_next_ready_thread(&active_thread);

    if (next_thread != nullptr && find_pending_channel_by_server_thread(&active_thread) == nullptr)
    {
      active_thread.prepare_syscall_resume(STATUS_OK);
      runtime.set_current_thread(next_thread);
    }

    return STATUS_OK;
  }

  case SYSCALL_RPC_OPEN:
  {
    if (syscall_context.argument0 == 0 || syscall_context.argument1 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    char endpoint_name[RPC_ENDPOINT_NAME_BUFFER_SIZE];
    memset(endpoint_name, 0, sizeof(endpoint_name));
    const int32_t name_status = runtime.copy_user_string(
      active_thread, static_cast<uintptr_t>(syscall_context.argument0), endpoint_name, sizeof(endpoint_name));

    if (name_status != STATUS_OK)
    {
      return name_status;
    }

    rpc_endpoint* endpoint = find_endpoint_by_name(endpoint_name);

    if (endpoint == nullptr)
    {
      return STATUS_NOT_FOUND;
    }

    thread* server_thread = endpoint->get_owner_thread();

    if (server_thread == nullptr || server_thread->get_state() == USER_THREAD_STATE_EXITED)
    {
      return STATUS_PEER_CLOSED;
    }

    rpc_channel* channel = m_channels.emplace(*owner_process, *endpoint);

    if (channel == nullptr)
    {
      return STATUS_NO_MEMORY;
    }

    runtime.grant_process_access(*channel);
    const handle_t channel_handle = channel->get_handle();
    const int32_t write_status = runtime.write_user_bytes(
      *owner_process, static_cast<uintptr_t>(syscall_context.argument1), &channel_handle, sizeof(channel_handle));

    if (write_status != STATUS_OK)
    {
      (void) m_channels.erase_by_handle(channel_handle);
      return write_status;
    }

    return STATUS_OK;
  }

  case SYSCALL_RPC_CALL:
  {
    if (syscall_context.argument0 == 0 || syscall_context.argument1 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    const handle_t channel_handle = static_cast<handle_t>(syscall_context.argument0);
    rpc_channel* channel = find_channel_by_handle(channel_handle);

    if (channel == nullptr)
    {
      return runtime.find_object_by_handle(channel_handle) == nullptr ? STATUS_BAD_HANDLE : STATUS_WRONG_TYPE;
    }

    if (!channel->is_owned_by(*owner_process))
    {
      return STATUS_BAD_HANDLE;
    }

    rpc_endpoint* endpoint = channel->get_endpoint();

    if (endpoint == nullptr)
    {
      return STATUS_PEER_CLOSED;
    }

    thread* server_thread = endpoint->get_owner_thread();

    if (server_thread == nullptr || server_thread->get_state() == USER_THREAD_STATE_EXITED)
    {
      return STATUS_PEER_CLOSED;
    }

    if (!endpoint->is_in_use() || endpoint->is_busy() || channel->is_busy())
    {
      return STATUS_WOULD_BLOCK;
    }

    ringos_rpc_request request {};
    const int32_t request_status = runtime.copy_user_bytes(
      *owner_process, static_cast<uintptr_t>(syscall_context.argument1), &request, sizeof(request));

    if (request_status != STATUS_OK)
    {
      return request_status;
    }

    if (request.operation == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    const int32_t prepare_status = prepare_server_thread(runtime, *endpoint, *server_thread, request);

    if (prepare_status != STATUS_OK)
    {
      return prepare_status;
    }

    channel->begin_call(active_thread, *server_thread);

    if (server_thread != &active_thread)
    {
      active_thread.set_state(USER_THREAD_STATE_BLOCKED);
      runtime.set_current_thread(server_thread);
    }

    return STATUS_OK;
  }

  case SYSCALL_RPC_CLOSE:
  {
    if (syscall_context.argument0 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    const handle_t channel_handle = static_cast<handle_t>(syscall_context.argument0);
    rpc_channel* channel = find_channel_by_handle(channel_handle);

    if (channel == nullptr)
    {
      return runtime.find_object_by_handle(channel_handle) == nullptr ? STATUS_BAD_HANDLE : STATUS_WRONG_TYPE;
    }

    if (!channel->is_owned_by(*owner_process))
    {
      return STATUS_BAD_HANDLE;
    }

    if (channel->is_busy())
    {
      return STATUS_BAD_STATE;
    }

    return m_channels.erase_by_handle(channel_handle) ? STATUS_OK : STATUS_BAD_STATE;
  }

  case SYSCALL_RPC_COMPLETE:
  {
    return complete_call(runtime, active_thread, static_cast<int32_t>(syscall_context.argument0));
  }

  default:
  {
    return STATUS_NOT_SUPPORTED;
  }
  }
}

rpc_channel* rpc_runtime::find_channel_by_handle(handle_t handle_value)
{
  return m_channels.find_by_handle(handle_value);
}

rpc_endpoint* rpc_runtime::find_endpoint_by_name(const char* endpoint_name)
{
  if (endpoint_name == nullptr)
  {
    return nullptr;
  }

  for (uint32_t index = 0; index < USER_RUNTIME_MAX_RPC_ENDPOINTS; ++index)
  {
    rpc_endpoint& endpoint = m_endpoints[index];

    if (endpoint.matches_name(endpoint_name))
    {
      return &endpoint;
    }
  }

  return nullptr;
}

rpc_endpoint* rpc_runtime::find_free_endpoint()
{
  for (uint32_t index = 0; index < USER_RUNTIME_MAX_RPC_ENDPOINTS; ++index)
  {
    if (!m_endpoints[index].is_in_use())
    {
      return &m_endpoints[index];
    }
  }

  return nullptr;
}

rpc_channel* rpc_runtime::find_pending_channel_by_server_thread(thread* server_thread)
{
  if (server_thread == nullptr)
  {
    return nullptr;
  }

  for (uint32_t index = 0; index < USER_RUNTIME_MAX_RPC_CHANNELS; ++index)
  {
    rpc_channel* channel = m_channels.get_by_index(index);

    if (channel != nullptr && channel->get_pending_server_thread() == server_thread)
    {
      return channel;
    }
  }

  return nullptr;
}

void rpc_runtime::clear_endpoints_owned_by(thread& owner_thread)
{
  for (uint32_t index = 0; index < USER_RUNTIME_MAX_RPC_ENDPOINTS; ++index)
  {
    rpc_endpoint& endpoint = m_endpoints[index];

    if (endpoint.get_owner_thread() == &owner_thread)
    {
      endpoint.clear();
    }
  }
}

int32_t rpc_runtime::complete_call(user_runtime& runtime, thread& server_thread, int32_t callback_status)
{
  rpc_channel* channel = find_pending_channel_by_server_thread(&server_thread);

  if (channel == nullptr)
  {
    return STATUS_BAD_STATE;
  }

  thread* client_thread = channel->get_pending_client_thread();
  const bool is_same_thread_call = client_thread == &server_thread;
  channel->finish_call();

  if (is_same_thread_call)
  {
    return callback_status;
  }

  if (client_thread == nullptr)
  {
    return STATUS_BAD_STATE;
  }

  client_thread->set_state(USER_THREAD_STATE_READY);
  client_thread->prepare_syscall_resume(callback_status);
  runtime.set_current_thread(client_thread);
  return STATUS_OK;
}

int32_t rpc_runtime::fail_pending_call(thread& server_thread, int32_t failure_status)
{
  rpc_channel* channel = find_pending_channel_by_server_thread(&server_thread);

  if (channel == nullptr)
  {
    return STATUS_OK;
  }

  thread* client_thread = channel->get_pending_client_thread();
  const bool is_same_thread_call = client_thread == &server_thread;
  channel->cancel_call();

  if (!is_same_thread_call && client_thread != nullptr)
  {
    client_thread->set_state(USER_THREAD_STATE_READY);
    client_thread->prepare_syscall_resume(failure_status);
  }

  return failure_status;
}

int32_t rpc_runtime::prepare_server_thread(
  user_runtime& runtime, const rpc_endpoint& endpoint, thread& server_thread, const ringos_rpc_request& request)
{
  process* server_process = endpoint.get_owner_process();

  if (server_process == nullptr)
  {
    return STATUS_BAD_STATE;
  }

  const uintptr_t callback_address = endpoint.get_callback_address();
  const uintptr_t completion_address = endpoint.get_completion_address();

  if (
    callback_address == 0 || completion_address == 0
    || !runtime.validate_user_range(*server_process, callback_address, 1)
    || !runtime.validate_user_range(*server_process, completion_address, 1))
  {
    return STATUS_FAULT;
  }

  const uintptr_t saved_stack_pointer = server_thread.get_user_context().stack_pointer;

  if (saved_stack_pointer == 0)
  {
    return STATUS_BAD_STATE;
  }

#if defined(__x86_64__)
  if (saved_stack_pointer <= sizeof(ringos_rpc_request) + sizeof(uintptr_t))
  {
    return STATUS_FAULT;
  }

  const uintptr_t request_address = align_down(saved_stack_pointer - sizeof(ringos_rpc_request), 16);

  if (request_address <= sizeof(uintptr_t))
  {
    return STATUS_FAULT;
  }

  const uintptr_t return_address = request_address - sizeof(uintptr_t);
  const int32_t request_status = runtime.write_user_bytes(*server_process, request_address, &request, sizeof(request));

  if (request_status != STATUS_OK)
  {
    return request_status;
  }

  const int32_t return_status
    = runtime.write_user_bytes(*server_process, return_address, &completion_address, sizeof(completion_address));

  if (return_status != STATUS_OK)
  {
    return return_status;
  }

  server_thread.prepare_rpc_resume(callback_address, completion_address, request_address, return_address);
  return STATUS_OK;
#elif defined(__aarch64__)
  if (saved_stack_pointer <= sizeof(ringos_rpc_request))
  {
    return STATUS_FAULT;
  }

  const uintptr_t request_address = align_down(saved_stack_pointer - sizeof(ringos_rpc_request), 16);
  const int32_t request_status = runtime.write_user_bytes(*server_process, request_address, &request, sizeof(request));

  if (request_status != STATUS_OK)
  {
    return request_status;
  }

  server_thread.prepare_rpc_resume(callback_address, completion_address, request_address, request_address);
  return STATUS_OK;
#else
  (void) runtime;
  (void) server_thread;
  (void) request;
  return STATUS_NOT_SUPPORTED;
#endif
}
