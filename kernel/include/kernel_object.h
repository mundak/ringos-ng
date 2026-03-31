#pragma once

#include "handle.h"

#include <stdint.h>

class kernel_object
{
public:
  uint32_t get_id() const;
  handle_t get_handle() const;
  uint32_t get_process_reference_count() const;

protected:
  kernel_object(uint32_t object_id, handle_t handle_value);
  ~kernel_object();
  void acquire_process_reference();

private:
  friend class user_runtime;

  uint32_t m_id;
  handle_t m_handle_value;
  uint32_t m_process_reference_count;
};

