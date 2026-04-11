#include "user_runtime.h"

#include "arch_user_runtime.h"
#include "debug.h"
#include "memory.h"
#include "panic.h"
#include "x64_windows_compat.h"

#include <ringos/rpc.h>

namespace
{
  constexpr uint64_t WINDOWS_HANDLE_STDIN = 1;
  constexpr uint64_t WINDOWS_HANDLE_STDOUT = 2;
  constexpr uint64_t WINDOWS_HANDLE_STDERR = 3;
  constexpr size_t WINDOWS_LOG_BUFFER_SIZE = 96;

  user_runtime g_user_runtime {};

  bool is_windows_console_handle(uint64_t handle_value)
  {
    return handle_value == WINDOWS_HANDLE_STDOUT || handle_value == WINDOWS_HANDLE_STDERR;
  }
}

void user_runtime::reset()
{
  m_current_thread = nullptr;
  __atomic_store_n(&m_next_handle_value, static_cast<handle_t>(1), __ATOMIC_RELAXED);
  m_processes.reset(m_next_handle_value);
  m_threads.reset(m_next_handle_value);
  m_channels.reset(m_next_handle_value);
  m_shared_memory_objects.reset(m_next_handle_value);
}

process* user_runtime::create_process(const address_space& address_space_info)
{
  process* current_process = m_processes.emplace(address_space_info);

  if (current_process == nullptr)
  {
    return nullptr;
  }

  grant_process_access(*current_process);
  return current_process;
}

thread* user_runtime::create_thread(
  process& process_context, const thread_context& initial_context, handle_t* out_thread_handle)
{
  if (out_thread_handle == nullptr)
  {
    return nullptr;
  }

  thread* current_thread = m_threads.emplace(process_context, initial_context);

  if (current_thread == nullptr)
  {
    return nullptr;
  }

  grant_process_access(*current_thread);
  *out_thread_handle = current_thread->get_handle();
  return current_thread;
}

bool user_runtime::create_channel_pair(
  handle_t* out_first_handle, handle_t* out_second_handle, channel** out_first_channel, channel** out_second_channel)
{
  if (out_first_handle == nullptr || out_second_handle == nullptr)
  {
    return false;
  }

  if (!m_channels.has_free_items(2))
  {
    return false;
  }

  channel* first_channel = m_channels.emplace(static_cast<channel*>(nullptr));

  if (first_channel == nullptr)
  {
    return false;
  }

  channel* second_channel = m_channels.emplace(first_channel);

  if (second_channel == nullptr)
  {
    return false;
  }

  first_channel->set_peer(second_channel);

  grant_process_access(*first_channel);
  grant_process_access(*second_channel);
  *out_first_handle = first_channel->get_handle();
  *out_second_handle = second_channel->get_handle();

  if (out_first_channel != nullptr)
  {
    *out_first_channel = first_channel;
  }

  if (out_second_channel != nullptr)
  {
    *out_second_channel = second_channel;
  }

  return true;
}

shared_memory_object* user_runtime::create_shared_memory_object(
  uintptr_t user_address, size_t size, handle_t* out_handle)
{
  if (out_handle == nullptr)
  {
    return nullptr;
  }

  shared_memory_object* current_object = m_shared_memory_objects.emplace(user_address, size);

  if (current_object == nullptr)
  {
    return nullptr;
  }

  grant_process_access(*current_object);
  *out_handle = current_object->get_handle();
  return current_object;
}

process* user_runtime::find_process_by_handle(handle_t handle_value)
{
  return m_processes.find_by_handle(handle_value);
}

thread* user_runtime::find_thread_by_handle(handle_t handle_value)
{
  return m_threads.find_by_handle(handle_value);
}

channel* user_runtime::find_channel_by_handle(handle_t handle_value)
{
  return m_channels.find_by_handle(handle_value);
}

shared_memory_object* user_runtime::find_shared_memory_object_by_handle(handle_t handle_value)
{
  return m_shared_memory_objects.find_by_handle(handle_value);
}

kernel_object* user_runtime::find_object_by_handle(handle_t handle_value)
{
  process* process_object = find_process_by_handle(handle_value);

  if (process_object != nullptr)
  {
    return process_object;
  }

  thread* thread_object = find_thread_by_handle(handle_value);

  if (thread_object != nullptr)
  {
    return thread_object;
  }

  channel* channel_object = find_channel_by_handle(handle_value);

  if (channel_object != nullptr)
  {
    return channel_object;
  }

  return find_shared_memory_object_by_handle(handle_value);
}

bool user_runtime::validate_user_range(const process& owner_process, uintptr_t user_address, size_t length) const
{
  const uintptr_t user_base = owner_process.get_address_space_info().user_base;
  const size_t user_size = owner_process.get_address_space_info().user_size;

  if (user_size > static_cast<size_t>(UINTPTR_MAX - user_base))
  {
    return false;
  }

  const uintptr_t user_limit = user_base + user_size;

  if (user_address < user_base || user_address > user_limit)
  {
    return false;
  }

  if (length == 0)
  {
    return true;
  }

  if (static_cast<size_t>(user_limit - user_address) < length)
  {
    return false;
  }

  return true;
}

bool user_runtime::try_translate_user_address(
  const process& owner_process, uintptr_t user_address, size_t length, uintptr_t* out_host_address) const
{
  if (out_host_address == nullptr)
  {
    return false;
  }

  if (!validate_user_range(owner_process, user_address, length))
  {
    return false;
  }

  const uintptr_t user_base = owner_process.get_address_space_info().user_base;
  const uintptr_t user_host_base = owner_process.get_address_space_info().user_host_base;
  const uintptr_t translated_offset = user_address - user_base;

  if (user_host_base == 0 || translated_offset > UINTPTR_MAX - user_host_base)
  {
    return false;
  }

  *out_host_address = user_host_base + translated_offset;
  return true;
}

int32_t user_runtime::copy_user_bytes(
  const process& owner_process, uintptr_t user_address, void* buffer, size_t buffer_size) const
{
  if (buffer == nullptr)
  {
    return STATUS_INVALID_ARGUMENT;
  }

  if (buffer_size == 0)
  {
    return STATUS_OK;
  }

  uintptr_t host_address = 0;

  if (!try_translate_user_address(owner_process, user_address, buffer_size, &host_address))
  {
    return STATUS_FAULT;
  }

  memcpy(buffer, reinterpret_cast<const void*>(host_address), buffer_size);
  return STATUS_OK;
}

int32_t user_runtime::write_user_bytes(
  const process& owner_process, uintptr_t user_address, const void* buffer, size_t buffer_size) const
{
  if (buffer == nullptr)
  {
    return STATUS_INVALID_ARGUMENT;
  }

  if (buffer_size == 0)
  {
    return STATUS_OK;
  }

  uintptr_t host_address = 0;

  if (!try_translate_user_address(owner_process, user_address, buffer_size, &host_address))
  {
    return STATUS_FAULT;
  }

  memcpy(reinterpret_cast<void*>(host_address), buffer, buffer_size);
  return STATUS_OK;
}

int32_t user_runtime::copy_user_string(
  const thread& owner_thread, uintptr_t user_address, char* buffer, size_t buffer_size) const
{
  if (owner_thread.get_process_context() == nullptr)
  {
    return STATUS_BAD_STATE;
  }

  if (buffer == nullptr || buffer_size == 0)
  {
    return STATUS_INVALID_ARGUMENT;
  }

  for (size_t index = 0; index < buffer_size; ++index)
  {
    const uintptr_t current_address = user_address + index;
    uintptr_t host_address = 0;

    if (!try_translate_user_address(*owner_thread.get_process_context(), current_address, 1, &host_address))
    {
      return STATUS_FAULT;
    }

    const char current_byte = *reinterpret_cast<const char*>(host_address);
    buffer[index] = current_byte;

    if (current_byte == '\0')
    {
      return STATUS_OK;
    }
  }

  buffer[buffer_size - 1] = '\0';
  return STATUS_BUFFER_TOO_SMALL;
}

bool user_runtime::copy_rpc_transfer_payload(
  const process& source_process,
  const ringos_rpc_request& source_request,
  const process& target_process,
  ringos_rpc_request* out_target_request)
{
  if (out_target_request == nullptr)
  {
    return false;
  }

  *out_target_request = source_request;

  if (source_request.argument0 == 0 || source_request.argument1 == 0)
  {
    return true;
  }

  const address_space& target_address_space = target_process.get_address_space_info();
  const size_t payload_size = static_cast<size_t>(source_request.argument1);

  if (target_address_space.rpc_transfer_host_address == 0 || payload_size > target_address_space.rpc_transfer_size)
  {
    return false;
  }

  const int32_t copy_status = copy_user_bytes(
    source_process,
    static_cast<uintptr_t>(source_request.argument0),
    reinterpret_cast<void*>(target_address_space.rpc_transfer_host_address),
    payload_size);

  if (copy_status != STATUS_OK)
  {
    return false;
  }

  out_target_request->argument0 = target_address_space.rpc_transfer_user_address;
  return true;
}

thread* user_runtime::get_current_thread()
{
  return m_current_thread;
}

thread* user_runtime::find_next_ready_thread(thread* after_thread)
{
  uint32_t start_index = 0;

  if (after_thread != nullptr)
  {
    for (uint32_t index = 0; index < USER_RUNTIME_MAX_THREADS; ++index)
    {
      if (m_threads.get_by_index(index) == after_thread)
      {
        start_index = index + 1;
        break;
      }
    }
  }

  for (uint32_t pass = 0; pass < 2; ++pass)
  {
    const uint32_t lower_bound = pass == 0 ? start_index : 0;
    const uint32_t upper_bound = pass == 0 ? USER_RUNTIME_MAX_THREADS : start_index;

    for (uint32_t index = lower_bound; index < upper_bound; ++index)
    {
      thread* candidate = m_threads.get_by_index(index);

      if (candidate != nullptr && candidate->get_state() == USER_THREAD_STATE_READY)
      {
        return candidate;
      }
    }
  }

  return nullptr;
}

bool user_runtime::schedule_next_ready_thread()
{
  thread* next_thread = find_next_ready_thread(m_current_thread);

  if (next_thread == nullptr)
  {
    m_current_thread = nullptr;
    return false;
  }

  set_current_thread(next_thread);
  return true;
}

void user_runtime::set_current_thread(thread* current_thread)
{
  if (
    m_current_thread != nullptr && m_current_thread != current_thread
    && m_current_thread->get_state() == USER_THREAD_STATE_RUNNING)
  {
    m_current_thread->set_state(USER_THREAD_STATE_READY);
  }

  m_current_thread = current_thread;
  const process* active_process = m_current_thread != nullptr ? m_current_thread->get_process_context() : nullptr;
  arch_activate_process_address_space(active_process);
  arch_prepare_user_thread(m_current_thread);

  if (m_current_thread != nullptr && m_current_thread->get_state() == USER_THREAD_STATE_READY)
  {
    m_current_thread->set_state(USER_THREAD_STATE_RUNNING);
  }
}

int32_t user_runtime::dispatch_syscall(const user_syscall_context& syscall_context)
{
  thread* active_thread = get_current_thread();

  if (active_thread == nullptr)
  {
    return STATUS_BAD_STATE;
  }

  process* owner_process = active_thread->get_process_context();

  if (owner_process == nullptr)
  {
    return STATUS_BAD_STATE;
  }

  const auto dispatch_rpc_call = [&](channel* channel_object, uintptr_t request_address, uintptr_t response_address)
  {
    if (channel_object == nullptr)
    {
      return STATUS_NOT_FOUND;
    }

    channel* const server_channel = channel_object->get_peer();

    if (server_channel == nullptr)
    {
      return STATUS_PEER_CLOSED;
    }

    if (server_channel->m_waiting_thread == nullptr || server_channel->m_wait_request_address == 0)
    {
      return STATUS_WOULD_BLOCK;
    }

    process* const server_process = server_channel->m_waiting_thread->get_process_context();

    if (server_process == nullptr)
    {
      return STATUS_BAD_STATE;
    }

    ringos_rpc_request request {};
    const int32_t request_status = copy_user_bytes(*owner_process, request_address, &request, sizeof(request));

    if (request_status != STATUS_OK)
    {
      return request_status;
    }

    if (request.operation == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    ringos_rpc_request server_request {};

    if (!copy_rpc_transfer_payload(*owner_process, request, *server_process, &server_request))
    {
      return STATUS_BUFFER_TOO_SMALL;
    }

    const int32_t write_status = write_user_bytes(
      *server_process, server_channel->m_wait_request_address, &server_request, sizeof(server_request));

    if (write_status != STATUS_OK)
    {
      return write_status;
    }

    active_thread->set_state(USER_THREAD_STATE_BLOCKED);
    server_channel->m_pending_client_thread = active_thread;
    server_channel->m_pending_client_response_address = response_address;
    server_channel->m_pending_request = server_request;
    thread* server_thread = server_channel->m_waiting_thread;
    server_thread->set_pending_syscall_status(STATUS_OK);
    server_thread->set_state(USER_THREAD_STATE_READY);
    server_channel->m_waiting_thread = nullptr;
    server_channel->m_wait_request_address = 0;
    set_current_thread(server_thread);
    return STATUS_OK;
  };

  switch (syscall_context.syscall_number)
  {
  case SYSCALL_DEBUG_LOG:
  {
    if (syscall_context.argument0 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    char message_buffer[96];
    memset(message_buffer, 0, sizeof(message_buffer));
    const int32_t copy_status = copy_user_string(
      *active_thread, static_cast<uintptr_t>(syscall_context.argument0), message_buffer, sizeof(message_buffer));

    if (copy_status != STATUS_OK)
    {
      return copy_status;
    }

    debug_log(message_buffer);
    return STATUS_OK;
  }

  case SYSCALL_THREAD_EXIT:
  {
    active_thread->set_state(USER_THREAD_STATE_EXITED);
    active_thread->set_exit_status(syscall_context.argument0);
    schedule_next_ready_thread();
    return STATUS_OK;
  }

  case SYSCALL_RPC_CALL:
  {
    if (syscall_context.argument0 == 0 || syscall_context.argument1 == 0 || syscall_context.argument2 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    const handle_t channel_handle = static_cast<handle_t>(syscall_context.argument0);
    kernel_object* const object = find_object_by_handle(channel_handle);

    if (object == nullptr)
    {
      return STATUS_BAD_HANDLE;
    }

    channel* const channel_object = find_channel_by_handle(channel_handle);

    if (channel_object == nullptr)
    {
      return STATUS_WRONG_TYPE;
    }

    return dispatch_rpc_call(
      channel_object,
      static_cast<uintptr_t>(syscall_context.argument1),
      static_cast<uintptr_t>(syscall_context.argument2));
  }

  case SYSCALL_RPC_WAIT:
  {
    if (syscall_context.argument0 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    return STATUS_NOT_FOUND;
  }

  case SYSCALL_RPC_REPLY:
  {
    if (syscall_context.argument0 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    return STATUS_NOT_FOUND;
  }

  case SYSCALL_DEVICE_MEMORY_MAP:
  {
    if (syscall_context.argument0 == 0 || syscall_context.argument1 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    return STATUS_NOT_FOUND;
  }

  case SYSCALL_RPC_OPEN:
  {
    if (syscall_context.argument0 == 0 || syscall_context.argument1 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    return STATUS_NOT_FOUND;
  }

  case SYSCALL_CONSOLE_QUERY:
  {
    if (syscall_context.argument2 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    const size_t device_capacity = static_cast<size_t>(syscall_context.argument1);

    if (device_capacity != 0 && syscall_context.argument0 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    const size_t available_device_count = 0;
    const int32_t count_status = write_user_bytes(
      *owner_process,
      static_cast<uintptr_t>(syscall_context.argument2),
      &available_device_count,
      sizeof(available_device_count));

    if (count_status != STATUS_OK)
    {
      return count_status;
    }

    if (available_device_count == 0)
    {
      return STATUS_OK;
    }

    return device_capacity < available_device_count ? STATUS_BUFFER_TOO_SMALL : STATUS_OK;
  }

  case SYSCALL_WINDOWS_GET_STD_HANDLE:
  {
    const int32_t standard_handle = static_cast<int32_t>(syscall_context.argument0);

    if (standard_handle == X64_WINDOWS_STD_INPUT_HANDLE)
    {
      return static_cast<int32_t>(WINDOWS_HANDLE_STDIN);
    }

    if (standard_handle == X64_WINDOWS_STD_OUTPUT_HANDLE)
    {
      return static_cast<int32_t>(WINDOWS_HANDLE_STDOUT);
    }

    if (standard_handle == X64_WINDOWS_STD_ERROR_HANDLE)
    {
      return static_cast<int32_t>(WINDOWS_HANDLE_STDERR);
    }

    return -1;
  }

  case SYSCALL_WINDOWS_WRITE_FILE:
  {
    if (!is_windows_console_handle(syscall_context.argument0) || syscall_context.argument1 == 0)
    {
      return 0;
    }

    char write_buffer[WINDOWS_LOG_BUFFER_SIZE + 1];
    uintptr_t current_address = static_cast<uintptr_t>(syscall_context.argument1);
    size_t remaining_bytes = static_cast<size_t>(syscall_context.argument2);
    uint32_t total_written = 0;

    while (remaining_bytes > 0)
    {
      const size_t chunk_size = remaining_bytes < WINDOWS_LOG_BUFFER_SIZE ? remaining_bytes : WINDOWS_LOG_BUFFER_SIZE;
      const int32_t copy_status = copy_user_bytes(*owner_process, current_address, write_buffer, chunk_size);

      if (copy_status != STATUS_OK)
      {
        return 0;
      }

      write_buffer[chunk_size] = '\0';
      debug_log(write_buffer);
      current_address += chunk_size;
      remaining_bytes -= chunk_size;
      total_written += static_cast<uint32_t>(chunk_size);
    }

    if (syscall_context.argument3 != 0)
    {
      const int32_t write_status = write_user_bytes(
        *owner_process, static_cast<uintptr_t>(syscall_context.argument3), &total_written, sizeof(total_written));

      if (write_status != STATUS_OK)
      {
        return 0;
      }
    }

    return 1;
  }

  case SYSCALL_WINDOWS_EXIT_PROCESS:
  {
    active_thread->set_state(USER_THREAD_STATE_EXITED);
    active_thread->set_exit_status(syscall_context.argument0);
    schedule_next_ready_thread();
    return STATUS_OK;
  }

  default:
  {
    return STATUS_NOT_SUPPORTED;
  }
  }
}

bool user_runtime::has_runnable_thread() const
{
  return m_current_thread != nullptr && m_current_thread->get_state() != USER_THREAD_STATE_BLOCKED
    && m_current_thread->get_state() != USER_THREAD_STATE_EXITED;
}

void user_runtime::grant_process_access(kernel_object& object)
{
  object.acquire_process_reference();
}

user_runtime& get_kernel_user_runtime()
{
  return g_user_runtime;
}

[[noreturn]] void run_initial_user_runtime(initial_user_runtime_platform& platform)
{
  user_runtime& runtime = get_kernel_user_runtime();
  runtime.reset();

  initial_user_runtime_bootstrap bootstrap {};
  platform.initialize(platform.context, bootstrap);

  if (bootstrap.process_count == 0 || bootstrap.process_count > USER_RUNTIME_MAX_INITIAL_PROCESSES)
  {
    panic("invalid initial process count");
  }

  if (bootstrap.initial_process_index >= bootstrap.process_count)
  {
    panic("invalid initial process index");
  }

  process* processes[USER_RUNTIME_MAX_INITIAL_PROCESSES] {};
  thread* threads[USER_RUNTIME_MAX_INITIAL_PROCESSES] {};

  for (uint32_t index = 0; index < bootstrap.process_count; ++index)
  {
    processes[index] = runtime.create_process(bootstrap.address_space[index]);

    if (processes[index] == nullptr)
    {
      panic("failed to create initial process");
    }

    handle_t thread_handle = 0;
    threads[index] = runtime.create_thread(*processes[index], bootstrap.thread_context[index], &thread_handle);

    if (threads[index] == nullptr || thread_handle == 0)
    {
      panic("failed to create initial thread");
    }
  }

  thread* const initial_thread = threads[bootstrap.initial_process_index];
  process* const initial_process = processes[bootstrap.initial_process_index];
  runtime.set_current_thread(initial_thread);
  platform.prepare_thread_launch(platform.context, *initial_process, *initial_thread);
  platform.enter_user_thread(platform.context, *initial_process, *initial_thread);
  panic("initial user runtime platform returned unexpectedly");
}
