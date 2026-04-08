#include "qemu_arm64_virt_machine.h"

#include "memory.h"

namespace
{
  constexpr uint32_t FDT_MAGIC = 0xD00DFEED;
  constexpr uint32_t FDT_TOKEN_BEGIN_NODE = 0x00000001;
  constexpr uint32_t FDT_TOKEN_END_NODE = 0x00000002;
  constexpr uint32_t FDT_TOKEN_PROP = 0x00000003;
  constexpr uint32_t FDT_TOKEN_NOP = 0x00000004;
  constexpr uint32_t FDT_TOKEN_END = 0x00000009;
  constexpr uint32_t FDT_HEADER_SIZE = 40;
  constexpr uint32_t FDT_MAX_DEPTH = 16;
  constexpr uint32_t FDT_MAX_PATH_LENGTH = MACHINE_DEVICE_STRING_MAX_LENGTH;
  constexpr uintptr_t FDT_MMIO_PAGE_ALIGNMENT = 0x1000;

  struct fdt_view
  {
    const uint8_t* blob;
    size_t size;
    const uint8_t* struct_block;
    size_t struct_size;
    const char* strings_block;
    size_t strings_size;
  };

  struct fdt_machine_scan_result
  {
    bool is_qemu_virt = false;
    uint32_t root_address_cells = 2;
    uint32_t root_size_cells = 1;
    char stdout_path_or_alias[MACHINE_DEVICE_STRING_MAX_LENGTH] {};
    char stdout_alias_path[MACHINE_DEVICE_STRING_MAX_LENGTH] {};
  };

  struct fdt_console_node_result
  {
    bool found = false;
    uintptr_t mmio_base = 0;
    size_t mmio_size = 0;
    char path[MACHINE_DEVICE_STRING_MAX_LENGTH] {};
    char compatible[MACHINE_DEVICE_STRING_MAX_LENGTH] {};
  };

  uint32_t read_big_endian_u32(const uint8_t* buffer)
  {
    if (buffer == nullptr)
    {
      return 0;
    }

    return (static_cast<uint32_t>(buffer[0]) << 24) | (static_cast<uint32_t>(buffer[1]) << 16)
      | (static_cast<uint32_t>(buffer[2]) << 8) | static_cast<uint32_t>(buffer[3]);
  }

  size_t align_up_to_u32(size_t value)
  {
    return (value + 3U) & ~static_cast<size_t>(3U);
  }

  void copy_string(char* destination, size_t capacity, const char* source)
  {
    if (destination == nullptr || capacity == 0)
    {
      return;
    }

    size_t index = 0;

    while (source != nullptr && source[index] != '\0' && index + 1 < capacity)
    {
      destination[index] = source[index];
      ++index;
    }

    destination[index] = '\0';
  }

  void copy_string_with_length(char* destination, size_t capacity, const char* source, size_t length)
  {
    if (destination == nullptr || capacity == 0)
    {
      return;
    }

    size_t index = 0;

    while (source != nullptr && index < length && source[index] != '\0' && index + 1 < capacity)
    {
      destination[index] = source[index];
      ++index;
    }

    destination[index] = '\0';
  }

  bool strings_equal(const char* first, const char* second)
  {
    if (first == nullptr || second == nullptr)
    {
      return false;
    }

    for (size_t index = 0;; ++index)
    {
      if (first[index] != second[index])
      {
        return false;
      }

      if (first[index] == '\0')
      {
        return true;
      }
    }
  }

  bool string_list_contains(const char* values, size_t values_length, const char* target)
  {
    if (values == nullptr || target == nullptr)
    {
      return false;
    }

    size_t offset = 0;

    while (offset < values_length)
    {
      const char* current_value = values + offset;
      size_t current_length = 0;

      while (offset + current_length < values_length && current_value[current_length] != '\0')
      {
        ++current_length;
      }

      if (offset + current_length >= values_length)
      {
        return false;
      }

      if (strings_equal(current_value, target))
      {
        return true;
      }

      offset += current_length + 1;
    }

    return false;
  }

  void copy_device_tree_path_without_suffix(char* destination, size_t capacity, const char* source, size_t length)
  {
    if (destination == nullptr || capacity == 0)
    {
      return;
    }

    size_t trimmed_length = 0;

    while (trimmed_length < length && source[trimmed_length] != '\0' && source[trimmed_length] != ':')
    {
      ++trimmed_length;
    }

    copy_string_with_length(destination, capacity, source, trimmed_length);
  }

  const char* get_fdt_string(const fdt_view& view, uint32_t string_offset)
  {
    if (string_offset >= view.strings_size)
    {
      return nullptr;
    }

    return view.strings_block + string_offset;
  }

  bool initialize_fdt_view(const boot_info& info, fdt_view& out_view)
  {
    memset(&out_view, 0, sizeof(out_view));

    if (info.device_tree_blob_address == 0)
    {
      return false;
    }

    const uint8_t* const blob = reinterpret_cast<const uint8_t*>(info.device_tree_blob_address);
    const uint32_t magic = read_big_endian_u32(blob);

    if (magic != FDT_MAGIC)
    {
      return false;
    }

    const size_t total_size
      = info.device_tree_blob_size != 0 ? info.device_tree_blob_size : read_big_endian_u32(blob + 4);

    if (total_size < FDT_HEADER_SIZE)
    {
      return false;
    }

    const size_t struct_offset = read_big_endian_u32(blob + 8);
    const size_t strings_offset = read_big_endian_u32(blob + 12);
    const size_t struct_size = read_big_endian_u32(blob + 36);
    const size_t strings_size = read_big_endian_u32(blob + 32);

    if (
      struct_offset > total_size || strings_offset > total_size || struct_size > total_size || strings_size > total_size
      || struct_offset + struct_size > total_size || strings_offset + strings_size > total_size)
    {
      return false;
    }

    out_view.blob = blob;
    out_view.size = total_size;
    out_view.struct_block = blob + struct_offset;
    out_view.struct_size = struct_size;
    out_view.strings_block = reinterpret_cast<const char*>(blob + strings_offset);
    out_view.strings_size = strings_size;
    return true;
  }

  bool try_read_reg_value(
    const uint8_t* property_value,
    size_t property_length,
    uint32_t address_cells,
    uint32_t size_cells,
    uintptr_t& out_base,
    size_t& out_size)
  {
    if (property_value == nullptr)
    {
      return false;
    }

    if ((address_cells != 1 && address_cells != 2) || (size_cells != 1 && size_cells != 2))
    {
      return false;
    }

    const size_t required_length = static_cast<size_t>(address_cells + size_cells) * sizeof(uint32_t);

    if (property_length < required_length)
    {
      return false;
    }

    uint64_t base = 0;
    uint64_t size = 0;
    size_t offset = 0;

    for (uint32_t index = 0; index < address_cells; ++index)
    {
      base = (base << 32) | read_big_endian_u32(property_value + offset);
      offset += sizeof(uint32_t);
    }

    for (uint32_t index = 0; index < size_cells; ++index)
    {
      size = (size << 32) | read_big_endian_u32(property_value + offset);
      offset += sizeof(uint32_t);
    }

    if (size == 0 || base > UINTPTR_MAX || size > static_cast<uint64_t>(SIZE_MAX))
    {
      return false;
    }

    out_base = static_cast<uintptr_t>(base);
    out_size = static_cast<size_t>(size);
    return true;
  }

  bool scan_qemu_virt_machine_metadata(const fdt_view& view, fdt_machine_scan_result& out_result)
  {
    memset(&out_result, 0, sizeof(out_result));
    out_result.root_address_cells = 2;
    out_result.root_size_cells = 1;

    char current_path[FDT_MAX_PATH_LENGTH] {};
    size_t current_path_length = 0;
    size_t previous_path_lengths[FDT_MAX_DEPTH] {};
    uint32_t depth = 0;
    size_t cursor = 0;

    while (cursor + sizeof(uint32_t) <= view.struct_size)
    {
      const uint32_t token = read_big_endian_u32(view.struct_block + cursor);
      cursor += sizeof(uint32_t);

      if (token == FDT_TOKEN_BEGIN_NODE)
      {
        if (depth >= FDT_MAX_DEPTH)
        {
          return false;
        }

        previous_path_lengths[depth] = current_path_length;
        const char* const node_name = reinterpret_cast<const char*>(view.struct_block + cursor);
        size_t node_name_length = 0;

        while (cursor + node_name_length < view.struct_size && node_name[node_name_length] != '\0')
        {
          ++node_name_length;
        }

        if (cursor + node_name_length >= view.struct_size)
        {
          return false;
        }

        if (depth == 0)
        {
          current_path[0] = '/';
          current_path[1] = '\0';
          current_path_length = 1;
        }
        else
        {
          if (current_path_length == 0)
          {
            return false;
          }

          if (current_path_length > 1 && current_path_length + 1 < sizeof(current_path))
          {
            current_path[current_path_length] = '/';
            ++current_path_length;
          }

          size_t copy_index = 0;

          while (copy_index < node_name_length && current_path_length + 1 < sizeof(current_path))
          {
            current_path[current_path_length] = node_name[copy_index];
            ++current_path_length;
            ++copy_index;
          }

          current_path[current_path_length] = '\0';
        }

        cursor += align_up_to_u32(node_name_length + 1);
        ++depth;
        continue;
      }

      if (token == FDT_TOKEN_END_NODE)
      {
        if (depth == 0)
        {
          return false;
        }

        current_path_length = previous_path_lengths[depth - 1];
        current_path[current_path_length] = '\0';
        --depth;
        continue;
      }

      if (token == FDT_TOKEN_PROP)
      {
        if (cursor + (2 * sizeof(uint32_t)) > view.struct_size)
        {
          return false;
        }

        const uint32_t property_length = read_big_endian_u32(view.struct_block + cursor);
        const uint32_t string_offset = read_big_endian_u32(view.struct_block + cursor + sizeof(uint32_t));
        cursor += 2 * sizeof(uint32_t);

        if (cursor + align_up_to_u32(property_length) > view.struct_size)
        {
          return false;
        }

        const char* const property_name = get_fdt_string(view, string_offset);
        const uint8_t* const property_value = view.struct_block + cursor;

        if (property_name == nullptr)
        {
          return false;
        }

        if (strings_equal(current_path, "/") && strings_equal(property_name, "compatible"))
        {
          out_result.is_qemu_virt
            = string_list_contains(reinterpret_cast<const char*>(property_value), property_length, "linux,dummy-virt")
            || string_list_contains(reinterpret_cast<const char*>(property_value), property_length, "qemu,virt");
        }
        else if (
          strings_equal(current_path, "/") && strings_equal(property_name, "#address-cells")
          && property_length >= sizeof(uint32_t))
        {
          out_result.root_address_cells = read_big_endian_u32(property_value);
        }
        else if (
          strings_equal(current_path, "/") && strings_equal(property_name, "#size-cells")
          && property_length >= sizeof(uint32_t))
        {
          out_result.root_size_cells = read_big_endian_u32(property_value);
        }
        else if (
          strings_equal(current_path, "/chosen")
          && (strings_equal(property_name, "stdout-path") || strings_equal(property_name, "linux,stdout-path")))
        {
          copy_device_tree_path_without_suffix(
            out_result.stdout_path_or_alias,
            sizeof(out_result.stdout_path_or_alias),
            reinterpret_cast<const char*>(property_value),
            property_length);
        }
        else if (
          strings_equal(current_path, "/aliases") && out_result.stdout_path_or_alias[0] != '\0'
          && strings_equal(property_name, out_result.stdout_path_or_alias))
        {
          copy_device_tree_path_without_suffix(
            out_result.stdout_alias_path,
            sizeof(out_result.stdout_alias_path),
            reinterpret_cast<const char*>(property_value),
            property_length);
        }

        cursor += align_up_to_u32(property_length);
        continue;
      }

      if (token == FDT_TOKEN_NOP)
      {
        continue;
      }

      if (token == FDT_TOKEN_END)
      {
        return depth == 0;
      }

      return false;
    }

    return false;
  }

  bool scan_qemu_virt_console_node(
    const fdt_view& view, const fdt_machine_scan_result& machine_scan, fdt_console_node_result& out_console)
  {
    memset(&out_console, 0, sizeof(out_console));

    char resolved_stdout_path[MACHINE_DEVICE_STRING_MAX_LENGTH] {};

    if (machine_scan.stdout_alias_path[0] != '\0')
    {
      copy_string(resolved_stdout_path, sizeof(resolved_stdout_path), machine_scan.stdout_alias_path);
    }
    else if (machine_scan.stdout_path_or_alias[0] == '/')
    {
      copy_string(resolved_stdout_path, sizeof(resolved_stdout_path), machine_scan.stdout_path_or_alias);
    }

    char current_path[FDT_MAX_PATH_LENGTH] {};
    size_t current_path_length = 0;
    size_t previous_path_lengths[FDT_MAX_DEPTH] {};
    bool node_is_pl011[FDT_MAX_DEPTH] {};
    bool node_has_reg[FDT_MAX_DEPTH] {};
    uintptr_t node_reg_base[FDT_MAX_DEPTH] {};
    size_t node_reg_size[FDT_MAX_DEPTH] {};
    uint32_t depth = 0;
    size_t cursor = 0;
    fdt_console_node_result first_pl011 {};

    while (cursor + sizeof(uint32_t) <= view.struct_size)
    {
      const uint32_t token = read_big_endian_u32(view.struct_block + cursor);
      cursor += sizeof(uint32_t);

      if (token == FDT_TOKEN_BEGIN_NODE)
      {
        if (depth >= FDT_MAX_DEPTH)
        {
          return false;
        }

        previous_path_lengths[depth] = current_path_length;
        const char* const node_name = reinterpret_cast<const char*>(view.struct_block + cursor);
        size_t node_name_length = 0;

        while (cursor + node_name_length < view.struct_size && node_name[node_name_length] != '\0')
        {
          ++node_name_length;
        }

        if (cursor + node_name_length >= view.struct_size)
        {
          return false;
        }

        if (depth == 0)
        {
          current_path[0] = '/';
          current_path[1] = '\0';
          current_path_length = 1;
        }
        else
        {
          if (current_path_length == 0)
          {
            return false;
          }

          if (current_path_length > 1 && current_path_length + 1 < sizeof(current_path))
          {
            current_path[current_path_length] = '/';
            ++current_path_length;
          }

          size_t copy_index = 0;

          while (copy_index < node_name_length && current_path_length + 1 < sizeof(current_path))
          {
            current_path[current_path_length] = node_name[copy_index];
            ++current_path_length;
            ++copy_index;
          }

          current_path[current_path_length] = '\0';
        }

        node_is_pl011[depth] = false;
        node_has_reg[depth] = false;
        node_reg_base[depth] = 0;
        node_reg_size[depth] = 0;
        cursor += align_up_to_u32(node_name_length + 1);
        ++depth;
        continue;
      }

      if (token == FDT_TOKEN_END_NODE)
      {
        if (depth == 0)
        {
          return false;
        }

        const uint32_t node_index = depth - 1;

        if (node_is_pl011[node_index] && node_has_reg[node_index])
        {
          if (!first_pl011.found)
          {
            first_pl011.found = true;
            first_pl011.mmio_base = node_reg_base[node_index];
            first_pl011.mmio_size = node_reg_size[node_index];
            copy_string(first_pl011.path, sizeof(first_pl011.path), current_path);
            copy_string(first_pl011.compatible, sizeof(first_pl011.compatible), "arm,pl011");
          }

          if (resolved_stdout_path[0] != '\0' && strings_equal(current_path, resolved_stdout_path))
          {
            out_console.found = true;
            out_console.mmio_base = node_reg_base[node_index];
            out_console.mmio_size = node_reg_size[node_index];
            copy_string(out_console.path, sizeof(out_console.path), current_path);
            copy_string(out_console.compatible, sizeof(out_console.compatible), "arm,pl011");
          }
        }

        current_path_length = previous_path_lengths[node_index];
        current_path[current_path_length] = '\0';
        --depth;
        continue;
      }

      if (token == FDT_TOKEN_PROP)
      {
        if (cursor + (2 * sizeof(uint32_t)) > view.struct_size)
        {
          return false;
        }

        const uint32_t property_length = read_big_endian_u32(view.struct_block + cursor);
        const uint32_t string_offset = read_big_endian_u32(view.struct_block + cursor + sizeof(uint32_t));
        cursor += 2 * sizeof(uint32_t);

        if (cursor + align_up_to_u32(property_length) > view.struct_size || depth == 0)
        {
          return false;
        }

        const char* const property_name = get_fdt_string(view, string_offset);
        const uint8_t* const property_value = view.struct_block + cursor;
        const uint32_t node_index = depth - 1;

        if (property_name == nullptr)
        {
          return false;
        }

        if (strings_equal(property_name, "compatible"))
        {
          node_is_pl011[node_index]
            = string_list_contains(reinterpret_cast<const char*>(property_value), property_length, "arm,pl011");
        }
        else if (strings_equal(property_name, "reg"))
        {
          node_has_reg[node_index] = try_read_reg_value(
            property_value,
            property_length,
            machine_scan.root_address_cells,
            machine_scan.root_size_cells,
            node_reg_base[node_index],
            node_reg_size[node_index]);
        }

        cursor += align_up_to_u32(property_length);
        continue;
      }

      if (token == FDT_TOKEN_NOP)
      {
        continue;
      }

      if (token == FDT_TOKEN_END)
      {
        break;
      }

      return false;
    }

    if (!out_console.found && first_pl011.found)
    {
      out_console = first_pl011;
    }

    return out_console.found;
  }
}

bool try_initialize_qemu_arm64_virt_machine(const boot_info& info, machine_descriptor& out_machine)
{
  if (info.arch_id != ARCH_ARM64)
  {
    return false;
  }

  fdt_view device_tree {};

  if (!initialize_fdt_view(info, device_tree))
  {
    return false;
  }

  fdt_machine_scan_result machine_scan {};

  if (!scan_qemu_virt_machine_metadata(device_tree, machine_scan) || !machine_scan.is_qemu_virt)
  {
    return false;
  }

  fdt_console_node_result console_node {};

  if (!scan_qemu_virt_console_node(device_tree, machine_scan, console_node))
  {
    return false;
  }

  if ((console_node.mmio_base & (FDT_MMIO_PAGE_ALIGNMENT - 1U)) != 0)
  {
    return false;
  }

  out_machine.arch_id = info.arch_id;
  out_machine.machine_kind = MACHINE_KIND_QEMU_ARM64_VIRT;
  copy_string(out_machine.name, sizeof(out_machine.name), "qemu-arm64-virt");
  out_machine.console.register_model = MACHINE_CONSOLE_REGISTER_MODEL_PL011;
  out_machine.console.device_memory_type = DEVICE_MEMORY_TYPE_MMIO;
  out_machine.console.mmio_physical_address = console_node.mmio_base;
  out_machine.console.mmio_size = console_node.mmio_size;
  copy_string(out_machine.console.device_tree_path, sizeof(out_machine.console.device_tree_path), console_node.path);
  copy_string(out_machine.console.compatible, sizeof(out_machine.console.compatible), console_node.compatible);
  return true;
}
