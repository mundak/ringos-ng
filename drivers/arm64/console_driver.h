#pragma once

#include <cstddef>
#include <cstdint>
#include <ringos/rpc.h>

class arm64_console_driver
{
public:
  arm64_console_driver();

  static bool try_create(arm64_console_driver& out_driver);

  int32_t run() const;

private:
  struct device_layout;

  static int32_t map_device_memory(uintptr_t& out_base_address, size_t& out_size);

  void handle_request(const ringos_rpc_request& request, ringos_rpc_response& response) const;
  int32_t write_console_bytes(const char* buffer, size_t length, size_t& out_bytes_written) const;

  volatile device_layout* m_device_layout;
};
