#pragma once

#include "handle.h"
#include "memory.h"

#include <stdint.h>

template<typename object_t, uint32_t capacity>
class kernel_object_pool final
{
public:
  ~kernel_object_pool();

  void reset(handle_t& next_handle_value);

  template<typename... args_t>
  object_t* emplace(args_t&&... args);

  bool has_free_items(uint32_t required_count) const;

  object_t* find_by_handle(handle_t handle_value);
  const object_t* find_by_handle(handle_t handle_value) const;

private:
  struct slot
  {
    alignas(object_t) uint8_t storage[sizeof(object_t)] {};
    bool is_occupied = false;
  };

  object_t* get_item_at(uint32_t index);
  const object_t* get_item_at(uint32_t index) const;
  void destroy_occupied_items();
  handle_t allocate_handle_value();

  slot m_items[capacity] {};
  uint32_t m_next_object_id = 1;
  handle_t* m_next_handle_value = nullptr;
};

#include "kernel_object_pool.inl"
