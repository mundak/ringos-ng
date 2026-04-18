#pragma once

#include <stddef.h>
#include <stdint.h>

static constexpr uint32_t USER_RUNTIME_MAX_ADDRESS_SPACE_MAPPINGS = 8;

struct address_space_mapping
{
  uintptr_t user_address;
  uintptr_t host_address;
  size_t size;
};

struct address_space
{
  uintptr_t arch_root_table;
  uint32_t mapping_count;
  address_space_mapping mappings[USER_RUNTIME_MAX_ADDRESS_SPACE_MAPPINGS];
};

bool add_address_space_mapping(
  address_space& address_space_info, uintptr_t user_address, uintptr_t host_address, size_t size);
bool validate_address_space_range(const address_space& address_space_info, uintptr_t user_address, size_t length);
bool try_translate_address_space_address(
  const address_space& address_space_info, uintptr_t user_address, size_t length, uintptr_t* out_host_address);
bool try_translate_address_space_chunk(
  const address_space& address_space_info,
  uintptr_t user_address,
  size_t requested_length,
  uintptr_t* out_host_address,
  size_t* out_translated_length);
