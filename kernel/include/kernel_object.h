#pragma once

#include <stdint.h>

class kernel_object
{
public:
  bool is_in_use() const;
  uint32_t id() const;
  uint64_t handle_value() const;
  uint32_t process_reference_count() const;

protected:
  void clear_identity();
  void activate_identity(
    uint32_t object_id,
    uint64_t handle_value);
  void acquire_process_reference();

private:
  friend class user_runtime;

  bool m_in_use;
  uint32_t m_id;
  uint64_t m_handle_value;
  uint32_t m_process_reference_count;
};