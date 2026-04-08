#pragma once

#include <stddef.h>
#include <stdint.h>
#include <ringos/rpc.h>

class qemu_arm64_virt_console
{
public:
  qemu_arm64_virt_console();

  static bool try_create(qemu_arm64_virt_console& out_console);

  int32_t run() const;

private:
  static int32_t map_device_memory(uintptr_t& out_base_address, size_t& out_size);

  void handle_request(const ringos_rpc_request& request, ringos_rpc_response& response) const;
  bool is_transmit_fifo_full() const;
  void write_transmit_byte(char value) const;
  int32_t write_console_bytes(const char* buffer, size_t length, size_t& out_bytes_written) const;

  volatile uint8_t* m_mmio_base;
};
