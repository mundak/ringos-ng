#include "boot_info.h"
#include "kernel.h"
#include "klibc/memory.h"

namespace
{
  constexpr uint32_t FDT_MAGIC = 0xD00DFEED;

  uint32_t read_big_endian_u32(const uint8_t* buffer)
  {
    if (buffer == nullptr)
    {
      return 0;
    }

    return (static_cast<uint32_t>(buffer[0]) << 24) | (static_cast<uint32_t>(buffer[1]) << 16)
      | (static_cast<uint32_t>(buffer[2]) << 8) | static_cast<uint32_t>(buffer[3]);
  }

  size_t try_read_device_tree_blob_size(uintptr_t device_tree_blob_address)
  {
    if (device_tree_blob_address == 0)
    {
      return 0;
    }

    const uint8_t* const blob = reinterpret_cast<const uint8_t*>(device_tree_blob_address);

    if (read_big_endian_u32(blob) != FDT_MAGIC)
    {
      return 0;
    }

    return static_cast<size_t>(read_big_endian_u32(blob + 4));
  }
}

extern "C" [[noreturn]] void arm64_entry(uintptr_t device_tree_blob_address)
{
  run_global_constructors();

  boot_info info {};
  info.arch_id = ARCH_ARM64;
  info.device_tree_blob_address = device_tree_blob_address;
  info.device_tree_blob_size = try_read_device_tree_blob_size(device_tree_blob_address);

  kernel_main(info);
}
