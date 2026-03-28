#include "user_runtime.h"

#include "debug.h"
#include "memory.h"
#include "panic.h"

namespace
{

  user_runtime g_user_runtime {};

}

void user_runtime::reset()
{
  for (uint32_t index = 0; index < USER_RUNTIME_MAX_PROCESSES; ++index)
  {
    m_processes[index].clear();
  }

  for (uint32_t index = 0; index < USER_RUNTIME_MAX_THREADS; ++index)
  {
    m_threads[index].clear();
  }

  for (uint32_t index = 0; index < USER_RUNTIME_MAX_CHANNELS; ++index)
  {
    m_channels[index].clear();
  }

  for (uint32_t index = 0; index < USER_RUNTIME_MAX_SHARED_MEMORY_OBJECTS; ++index)
  {
    m_shared_memory_objects[index].clear();
  }

  m_current_thread = nullptr;
  m_next_process_id = 1;
  m_next_thread_id = 1;
  m_next_channel_id = 1;
  m_next_shared_memory_id = 1;
  m_next_handle_value = 1;
}

process* user_runtime::create_process(const address_space& address_space_info)
{
  for (uint32_t index = 0; index < USER_RUNTIME_MAX_PROCESSES; ++index)
  {
    process& current_process = m_processes[index];

    if (!current_process.is_in_use())
    {
      current_process.activate(m_next_process_id, allocate_handle_value(), address_space_info);
      ++m_next_process_id;
      grant_process_access(current_process);
      return &current_process;
    }
  }

  return nullptr;
}

thread* user_runtime::create_thread(
  process& process_context,
  const thread_context& initial_context,
  uint64_t* out_thread_handle)
{
  if (out_thread_handle == nullptr)
  {
    return nullptr;
  }

  for (uint32_t index = 0; index < USER_RUNTIME_MAX_THREADS; ++index)
  {
    thread& current_thread = m_threads[index];

    if (!current_thread.is_in_use())
    {
      current_thread.activate(m_next_thread_id, allocate_handle_value(), process_context, initial_context);
      ++m_next_thread_id;
      grant_process_access(current_thread);
      *out_thread_handle = current_thread.handle_value();
      return &current_thread;
    }
  }

  return nullptr;
}

bool user_runtime::create_channel_pair(
  uint64_t* out_first_handle,
  uint64_t* out_second_handle)
{
  if (out_first_handle == nullptr || out_second_handle == nullptr)
  {
    return false;
  }

  channel* first_channel = nullptr;
  channel* second_channel = nullptr;

  for (uint32_t index = 0; index < USER_RUNTIME_MAX_CHANNELS; ++index)
  {
    channel& current_channel = m_channels[index];

    if (!current_channel.is_in_use())
    {
      if (first_channel == nullptr)
      {
        first_channel = &current_channel;
      }
      else
      {
        second_channel = &current_channel;
        break;
      }
    }
  }

  if (first_channel == nullptr || second_channel == nullptr)
  {
    return false;
  }

  first_channel->activate(m_next_channel_id, allocate_handle_value(), second_channel);
  ++m_next_channel_id;
  second_channel->activate(m_next_channel_id, allocate_handle_value(), first_channel);
  ++m_next_channel_id;

  grant_process_access(*first_channel);
  grant_process_access(*second_channel);
  *out_first_handle = first_channel->handle_value();
  *out_second_handle = second_channel->handle_value();
  return true;
}

shared_memory_object* user_runtime::create_shared_memory_object(
  uintptr_t user_address,
  size_t size,
  uint64_t* out_handle)
{
  if (out_handle == nullptr)
  {
    return nullptr;
  }

  for (uint32_t index = 0; index < USER_RUNTIME_MAX_SHARED_MEMORY_OBJECTS; ++index)
  {
    shared_memory_object& current_object = m_shared_memory_objects[index];

    if (!current_object.is_in_use())
    {
      current_object.activate(m_next_shared_memory_id, allocate_handle_value(), user_address, size);
      ++m_next_shared_memory_id;

      grant_process_access(current_object);
      *out_handle = current_object.handle_value();
      return &current_object;
    }
  }

  return nullptr;
}

bool user_runtime::validate_user_range(
  const process& owner_process,
  uintptr_t user_address,
  size_t length) const
{
  const uintptr_t user_base = owner_process.address_space_info().user_base;
  const size_t user_size = owner_process.address_space_info().user_size;

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

int32_t user_runtime::copy_user_string(
  const thread& owner_thread,
  uintptr_t user_address,
  char* buffer,
  size_t buffer_size) const
{
  if (owner_thread.process_context() == nullptr)
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

    if (!validate_user_range(*owner_thread.process_context(), current_address, 1))
    {
      return STATUS_FAULT;
    }

    const char current_byte = *reinterpret_cast<const char*>(current_address);
    buffer[index] = current_byte;

    if (current_byte == '\0')
    {
      return STATUS_OK;
    }
  }

  buffer[buffer_size - 1] = '\0';
  return STATUS_BUFFER_TOO_SMALL;
}

thread* user_runtime::current_thread()
{
  return m_current_thread;
}

void user_runtime::set_current_thread(thread* current_thread)
{
  m_current_thread = current_thread;

  if (m_current_thread != nullptr)
  {
    m_current_thread->set_state(user_thread_state::running);
  }
}

int32_t user_runtime::dispatch_syscall(
  uint64_t syscall_number,
  uint64_t argument0)
{
  thread* active_thread = current_thread();

  if (active_thread == nullptr)
  {
    return STATUS_BAD_STATE;
  }

  switch (syscall_number)
  {
  case STAGE1_SYSCALL_DEBUG_LOG:
  {
    if (argument0 == 0)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    char message_buffer[96];
    memset(message_buffer, 0, sizeof(message_buffer));
    const int32_t copy_status = copy_user_string(
      *active_thread,
      static_cast<uintptr_t>(argument0),
      message_buffer,
      sizeof(message_buffer));

    if (copy_status != STATUS_OK)
    {
      return copy_status;
    }

    debug_log(message_buffer);
    return STATUS_OK;
  }

  case STAGE1_SYSCALL_THREAD_EXIT:
  {
    active_thread->set_state(user_thread_state::exited);
    active_thread->set_exit_status(argument0);
    return STATUS_OK;
  }

  default:
  {
    return STATUS_NOT_SUPPORTED;
  }
  }
}

bool user_runtime::should_resume_current_thread() const
{
  if (m_current_thread == nullptr)
  {
    return false;
  }

  return m_current_thread->state() != user_thread_state::exited;
}

uint64_t user_runtime::allocate_handle_value()
{
  const uint64_t handle_value = m_next_handle_value;
  ++m_next_handle_value;
  return handle_value;
}

void user_runtime::grant_process_access(kernel_object& object)
{
  object.acquire_process_reference();
}

user_runtime& kernel_user_runtime()
{
  return g_user_runtime;
}

[[noreturn]] void run_initial_user_runtime(initial_user_runtime_platform& platform)
{
  user_runtime& runtime = kernel_user_runtime();
  runtime.reset();

  initial_user_runtime_bootstrap bootstrap {};
  platform.initialize(platform.context, bootstrap);

  process* initial_process = runtime.create_process(bootstrap.address_space);

  if (initial_process == nullptr)
  {
    panic("failed to create initial process");
  }

  uint64_t thread_handle = 0;
  thread* initial_thread = runtime.create_thread(*initial_process, bootstrap.thread_context, &thread_handle);

  if (initial_thread == nullptr || thread_handle == 0)
  {
    panic("failed to create initial thread");
  }

  uint64_t first_channel_handle = 0;
  uint64_t second_channel_handle = 0;

  if (!runtime.create_channel_pair(&first_channel_handle, &second_channel_handle))
  {
    panic("failed to create initial channel pair");
  }

  uint64_t shared_memory_handle = 0;
  shared_memory_object* shared_memory = runtime.create_shared_memory_object(
    bootstrap.shared_memory_address,
    bootstrap.shared_memory_size,
    &shared_memory_handle);

  if (shared_memory == nullptr || shared_memory_handle == 0)
  {
    panic("failed to create initial shared memory object");
  }

  if (first_channel_handle == 0 || second_channel_handle == 0)
  {
    panic("failed to install initial channel handles");
  }

  runtime.set_current_thread(initial_thread);
  platform.prepare_thread_launch(platform.context, *initial_process, *initial_thread);
  platform.enter_user_thread(platform.context, *initial_process, *initial_thread);
  panic("initial user runtime platform returned unexpectedly");
}