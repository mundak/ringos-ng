#include "debug.h"
#include "memory.h"
#include "stage1_kernel.h"

namespace
{

  process g_processes[STAGE1_MAX_PROCESSES] {};
  thread g_threads[STAGE1_MAX_THREADS] {};
  channel g_channels[STAGE1_MAX_CHANNELS] {};
  shared_memory_object g_shared_memory_objects[STAGE1_MAX_SHARED_MEMORY_OBJECTS] {};

  thread* g_current_thread = nullptr;

  uint32_t g_next_process_id = 1;
  uint32_t g_next_thread_id = 1;
  uint32_t g_next_channel_id = 1;
  uint32_t g_next_shared_memory_id = 1;
  uint64_t g_next_handle_value = 1;

  handle_entry* install_handle(process& owner_process, object_type handle_object_type, void* object)
  {
    for (uint32_t index = 0; index < STAGE1_MAX_HANDLES; ++index)
    {
      handle_entry& current_entry = owner_process.m_handle_table[index];

      if (!current_entry.m_in_use)
      {
        current_entry.m_in_use = true;
        current_entry.m_handle = g_next_handle_value;
        current_entry.m_object_type = handle_object_type;
        current_entry.m_object = object;
        ++g_next_handle_value;
        return &current_entry;
      }
    }

    return nullptr;
  }

}

void stage1_reset_kernel_state()
{
  memset(g_processes, 0, sizeof(g_processes));
  memset(g_threads, 0, sizeof(g_threads));
  memset(g_channels, 0, sizeof(g_channels));
  memset(g_shared_memory_objects, 0, sizeof(g_shared_memory_objects));

  g_current_thread = nullptr;
  g_next_process_id = 1;
  g_next_thread_id = 1;
  g_next_channel_id = 1;
  g_next_shared_memory_id = 1;
  g_next_handle_value = 1;
}

process* stage1_create_process(const address_space& address_space_info)
{
  for (uint32_t index = 0; index < STAGE1_MAX_PROCESSES; ++index)
  {
    process& current_process = g_processes[index];

    if (!current_process.m_in_use)
    {
      memset(&current_process, 0, sizeof(current_process));
      current_process.m_in_use = true;
      current_process.m_id = g_next_process_id;
      current_process.m_address_space = address_space_info;
      ++g_next_process_id;
      return &current_process;
    }
  }

  return nullptr;
}

thread* stage1_create_thread(process& owner_process, const thread_context& initial_context, uint64_t* out_thread_handle)
{
  if (out_thread_handle == nullptr)
  {
    return nullptr;
  }

  for (uint32_t index = 0; index < STAGE1_MAX_THREADS; ++index)
  {
    thread& current_thread = g_threads[index];

    if (!current_thread.m_in_use)
    {
      memset(&current_thread, 0, sizeof(current_thread));
      current_thread.m_in_use = true;
      current_thread.m_state = thread_state::ready;
      current_thread.m_id = g_next_thread_id;
      current_thread.m_process = &owner_process;
      current_thread.m_user_context = initial_context;
      ++g_next_thread_id;

      handle_entry* thread_handle = install_handle(owner_process, object_type::thread, &current_thread);

      if (thread_handle == nullptr)
      {
        memset(&current_thread, 0, sizeof(current_thread));
        return nullptr;
      }

      *out_thread_handle = thread_handle->m_handle;
      return &current_thread;
    }
  }

  return nullptr;
}

bool stage1_create_channel_pair(process& owner_process, uint64_t* out_first_handle, uint64_t* out_second_handle)
{
  if (out_first_handle == nullptr || out_second_handle == nullptr)
  {
    return false;
  }

  channel* first_channel = nullptr;
  channel* second_channel = nullptr;

  for (uint32_t index = 0; index < STAGE1_MAX_CHANNELS; ++index)
  {
    channel& current_channel = g_channels[index];

    if (!current_channel.m_in_use)
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

  memset(first_channel, 0, sizeof(*first_channel));
  memset(second_channel, 0, sizeof(*second_channel));

  first_channel->m_in_use = true;
  first_channel->m_id = g_next_channel_id;
  first_channel->m_owner_process = &owner_process;
  first_channel->m_peer = second_channel;
  ++g_next_channel_id;

  second_channel->m_in_use = true;
  second_channel->m_id = g_next_channel_id;
  second_channel->m_owner_process = &owner_process;
  second_channel->m_peer = first_channel;
  ++g_next_channel_id;

  handle_entry* first_handle = install_handle(owner_process, object_type::channel, first_channel);
  handle_entry* second_handle = install_handle(owner_process, object_type::channel, second_channel);

  if (first_handle == nullptr || second_handle == nullptr)
  {
    memset(first_channel, 0, sizeof(*first_channel));
    memset(second_channel, 0, sizeof(*second_channel));
    return false;
  }

  *out_first_handle = first_handle->m_handle;
  *out_second_handle = second_handle->m_handle;
  return true;
}

shared_memory_object* stage1_create_shared_memory_object(
  process& owner_process, uintptr_t user_address, size_t size, uint64_t* out_handle)
{
  if (out_handle == nullptr)
  {
    return nullptr;
  }

  for (uint32_t index = 0; index < STAGE1_MAX_SHARED_MEMORY_OBJECTS; ++index)
  {
    shared_memory_object& current_object = g_shared_memory_objects[index];

    if (!current_object.m_in_use)
    {
      memset(&current_object, 0, sizeof(current_object));
      current_object.m_in_use = true;
      current_object.m_id = g_next_shared_memory_id;
      current_object.m_owner_process = &owner_process;
      current_object.m_user_address = user_address;
      current_object.m_size = size;
      ++g_next_shared_memory_id;

      handle_entry* handle = install_handle(owner_process, object_type::shared_memory, &current_object);

      if (handle == nullptr)
      {
        memset(&current_object, 0, sizeof(current_object));
        return nullptr;
      }

      *out_handle = handle->m_handle;
      return &current_object;
    }
  }

  return nullptr;
}

bool stage1_validate_user_range(const process& owner_process, uintptr_t user_address, size_t length)
{
  const uintptr_t user_base = owner_process.m_address_space.m_user_base;
  const uintptr_t user_limit = user_base + owner_process.m_address_space.m_user_size;

  if (user_address < user_base)
  {
    return false;
  }

  if (length == 0)
  {
    return user_address <= user_limit;
  }

  if (user_address > user_limit)
  {
    return false;
  }

  const uintptr_t end_address = user_address + length;

  if (end_address < user_address)
  {
    return false;
  }

  if (end_address > user_limit)
  {
    return false;
  }

  return true;
}

int32_t stage1_copy_user_string(const thread& owner_thread, uintptr_t user_address, char* buffer, size_t buffer_size)
{
  if (owner_thread.m_process == nullptr)
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

    if (!stage1_validate_user_range(*owner_thread.m_process, current_address, 1))
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

thread* stage1_get_current_thread()
{
  return g_current_thread;
}

void stage1_set_current_thread(thread* current_thread)
{
  g_current_thread = current_thread;

  if (g_current_thread != nullptr)
  {
    g_current_thread->m_state = thread_state::running;
  }
}

int32_t stage1_dispatch_syscall(uint64_t syscall_number, uint64_t argument0)
{
  thread* current_thread = stage1_get_current_thread();

  if (current_thread == nullptr)
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
    const int32_t copy_status = stage1_copy_user_string(
      *current_thread, static_cast<uintptr_t>(argument0), message_buffer, sizeof(message_buffer));

    if (copy_status != STATUS_OK)
    {
      return copy_status;
    }

    debug_log(message_buffer);
    return STATUS_OK;
  }

  case STAGE1_SYSCALL_THREAD_EXIT:
  {
    current_thread->m_state = thread_state::exited;
    current_thread->m_exit_status = argument0;
    return STATUS_OK;
  }

  default:
  {
    return STATUS_NOT_SUPPORTED;
  }
  }
}

bool stage1_should_resume_current_thread()
{
  thread* current_thread = stage1_get_current_thread();

  if (current_thread == nullptr)
  {
    return false;
  }

  return current_thread->m_state != thread_state::exited;
}