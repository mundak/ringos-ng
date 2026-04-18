#include "address_space.h"

namespace
{
  bool is_valid_address_range(uintptr_t base_address, size_t size)
  {
    return size != 0 && size <= static_cast<size_t>(UINTPTR_MAX - base_address);
  }

  bool ranges_overlap(uintptr_t left_address, size_t left_size, uintptr_t right_address, size_t right_size)
  {
    const uintptr_t left_limit = left_address + left_size;
    const uintptr_t right_limit = right_address + right_size;
    return left_address < right_limit && right_address < left_limit;
  }

  const address_space_mapping* find_address_space_mapping(
    const address_space& address_space_info, uintptr_t user_address, bool allow_mapping_limit)
  {
    for (uint32_t index = 0; index < address_space_info.mapping_count; ++index)
    {
      const address_space_mapping& mapping = address_space_info.mappings[index];

      if (mapping.size == 0)
      {
        continue;
      }

      const uintptr_t mapping_limit = mapping.user_address + mapping.size;

      if (user_address < mapping.user_address)
      {
        continue;
      }

      if (user_address < mapping_limit || (allow_mapping_limit && user_address == mapping_limit))
      {
        return &mapping;
      }
    }

    return nullptr;
  }
}

bool add_address_space_mapping(
  address_space& address_space_info, uintptr_t user_address, uintptr_t host_address, size_t size)
{
  if (!is_valid_address_range(user_address, size) || !is_valid_address_range(host_address, size))
  {
    return false;
  }

  if (address_space_info.mapping_count >= USER_RUNTIME_MAX_ADDRESS_SPACE_MAPPINGS)
  {
    return false;
  }

  for (uint32_t index = 0; index < address_space_info.mapping_count; ++index)
  {
    const address_space_mapping& existing_mapping = address_space_info.mappings[index];

    if (ranges_overlap(user_address, size, existing_mapping.user_address, existing_mapping.size))
    {
      return false;
    }
  }

  address_space_mapping& mapping = address_space_info.mappings[address_space_info.mapping_count];
  mapping.user_address = user_address;
  mapping.host_address = host_address;
  mapping.size = size;
  ++address_space_info.mapping_count;
  return true;
}

bool validate_address_space_range(const address_space& address_space_info, uintptr_t user_address, size_t length)
{
  if (length == 0)
  {
    return find_address_space_mapping(address_space_info, user_address, true) != nullptr;
  }

  uintptr_t current_address = user_address;
  size_t remaining_length = length;

  while (remaining_length != 0)
  {
    uintptr_t host_address = 0;
    size_t translated_length = 0;

    if (!try_translate_address_space_chunk(
          address_space_info, current_address, remaining_length, &host_address, &translated_length))
    {
      return false;
    }

    if (translated_length == 0 || translated_length > remaining_length)
    {
      return false;
    }

    if (translated_length > static_cast<size_t>(UINTPTR_MAX - current_address))
    {
      return false;
    }

    current_address += translated_length;
    remaining_length -= translated_length;
  }

  return true;
}

bool try_translate_address_space_address(
  const address_space& address_space_info, uintptr_t user_address, size_t length, uintptr_t* out_host_address)
{
  if (out_host_address == nullptr)
  {
    return false;
  }

  if (!validate_address_space_range(address_space_info, user_address, length))
  {
    return false;
  }

  size_t translated_length = 0;
  return try_translate_address_space_chunk(
           address_space_info, user_address, length, out_host_address, &translated_length)
    && translated_length == length;
}

bool try_translate_address_space_chunk(
  const address_space& address_space_info,
  uintptr_t user_address,
  size_t requested_length,
  uintptr_t* out_host_address,
  size_t* out_translated_length)
{
  if (out_host_address == nullptr || out_translated_length == nullptr)
  {
    return false;
  }

  const address_space_mapping* mapping
    = find_address_space_mapping(address_space_info, user_address, requested_length == 0);

  if (mapping == nullptr)
  {
    return false;
  }

  const uintptr_t translated_offset = user_address - mapping->user_address;

  if (mapping->host_address == 0 || translated_offset > UINTPTR_MAX - mapping->host_address)
  {
    return false;
  }

  const size_t available_length = mapping->size - static_cast<size_t>(translated_offset);

  if (requested_length != 0 && available_length == 0)
  {
    return false;
  }

  *out_host_address = mapping->host_address + translated_offset;
  *out_translated_length
    = requested_length == 0 || requested_length <= available_length ? requested_length : available_length;
  return true;
}
