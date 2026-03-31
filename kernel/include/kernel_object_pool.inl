template<typename object_t, uint32_t capacity>
kernel_object_pool<object_t, capacity>::~kernel_object_pool()
{
  destroy_occupied_items();
}

template<typename object_t, uint32_t capacity>
void kernel_object_pool<object_t, capacity>::reset(handle_t& next_handle_value)
{
  destroy_occupied_items();
  m_next_handle_value = &next_handle_value;
  m_next_object_id = 1;
}

template<typename object_t, uint32_t capacity>
template<typename... args_t>
object_t* kernel_object_pool<object_t, capacity>::emplace(args_t&&... args)
{
  if (m_next_handle_value == nullptr)
  {
    return nullptr;
  }

  for (uint32_t index = 0; index < capacity; ++index)
  {
    slot& current_slot = m_items[index];

    if (!current_slot.is_occupied)
    {
      object_t* current_item = new (&current_slot.storage[0])
        object_t(m_next_object_id, allocate_handle_value(), static_cast<args_t&&>(args)...);
      current_slot.is_occupied = true;
      ++m_next_object_id;
      return current_item;
    }
  }

  return nullptr;
}

template<typename object_t, uint32_t capacity>
bool kernel_object_pool<object_t, capacity>::has_free_items(uint32_t required_count) const
{
  uint32_t free_count = 0;

  for (uint32_t index = 0; index < capacity; ++index)
  {
    if (!m_items[index].is_occupied)
    {
      ++free_count;

      if (free_count >= required_count)
      {
        return true;
      }
    }
  }

  return required_count == 0;
}

template<typename object_t, uint32_t capacity>
object_t* kernel_object_pool<object_t, capacity>::find_by_handle(handle_t handle_value)
{
  for (uint32_t index = 0; index < capacity; ++index)
  {
    if (!m_items[index].is_occupied)
    {
      continue;
    }

    object_t* current_item = get_item_at(index);

    if (current_item->get_handle() == handle_value)
    {
      return current_item;
    }
  }

  return nullptr;
}

template<typename object_t, uint32_t capacity>
const object_t* kernel_object_pool<object_t, capacity>::find_by_handle(handle_t handle_value) const
{
  for (uint32_t index = 0; index < capacity; ++index)
  {
    if (!m_items[index].is_occupied)
    {
      continue;
    }

    const object_t* current_item = get_item_at(index);

    if (current_item->get_handle() == handle_value)
    {
      return current_item;
    }
  }

  return nullptr;
}

template<typename object_t, uint32_t capacity>
object_t* kernel_object_pool<object_t, capacity>::get_item_at(uint32_t index)
{
  return reinterpret_cast<object_t*>(&m_items[index].storage[0]);
}

template<typename object_t, uint32_t capacity>
const object_t* kernel_object_pool<object_t, capacity>::get_item_at(uint32_t index) const
{
  return reinterpret_cast<const object_t*>(&m_items[index].storage[0]);
}

template<typename object_t, uint32_t capacity>
void kernel_object_pool<object_t, capacity>::destroy_occupied_items()
{
  for (uint32_t index = 0; index < capacity; ++index)
  {
    if (!m_items[index].is_occupied)
    {
      continue;
    }

    get_item_at(index)->~object_t();
    m_items[index].is_occupied = false;
  }
}

template<typename object_t, uint32_t capacity>
handle_t kernel_object_pool<object_t, capacity>::allocate_handle_value()
{
  return __atomic_fetch_add(m_next_handle_value, static_cast<handle_t>(1), __ATOMIC_RELAXED);
}

