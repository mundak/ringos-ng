#include "x64_emulator.h"

#include "memory.h"

namespace
{
  constexpr uint64_t RFLAGS_RESERVED = 1ULL << 1;
  constexpr uint64_t RFLAGS_CARRY = 1ULL << 0;
  constexpr uint64_t RFLAGS_ZERO = 1ULL << 6;
  constexpr uint64_t RFLAGS_SIGN = 1ULL << 7;
  constexpr uint64_t RFLAGS_OVERFLOW = 1ULL << 11;

  uint64_t& get_register64(x64_emulator_state& state, uint32_t register_index)
  {
    return state.general_registers[register_index];
  }

  uint32_t get_register32(const x64_emulator_state& state, uint32_t register_index)
  {
    return static_cast<uint32_t>(state.general_registers[register_index]);
  }

  void set_register32(x64_emulator_state& state, uint32_t register_index, uint32_t value)
  {
    state.general_registers[register_index] = value;
  }

  bool translate_guest_pointer(
    const x64_emulator_memory& memory, uintptr_t guest_address, size_t length, uint8_t** out_pointer)
  {
    if (out_pointer == nullptr || memory.bytes == nullptr)
    {
      return false;
    }

    if (guest_address < memory.base_address)
    {
      return false;
    }

    const uintptr_t guest_offset = guest_address - memory.base_address;

    if (guest_offset > memory.size)
    {
      return false;
    }

    if (length > memory.size - static_cast<size_t>(guest_offset))
    {
      return false;
    }

    *out_pointer = memory.bytes + guest_offset;
    return true;
  }

  bool read_u8(const x64_emulator_memory& memory, uintptr_t guest_address, uint8_t* out_value)
  {
    uint8_t* source = nullptr;

    if (out_value == nullptr || !translate_guest_pointer(memory, guest_address, sizeof(uint8_t), &source))
    {
      return false;
    }

    *out_value = *source;
    return true;
  }

  bool read_i8(const x64_emulator_memory& memory, uintptr_t guest_address, int8_t* out_value)
  {
    uint8_t byte_value = 0;

    if (out_value == nullptr || !read_u8(memory, guest_address, &byte_value))
    {
      return false;
    }

    *out_value = static_cast<int8_t>(byte_value);
    return true;
  }

  bool read_u32(const x64_emulator_memory& memory, uintptr_t guest_address, uint32_t* out_value)
  {
    uint8_t* source = nullptr;

    if (out_value == nullptr || !translate_guest_pointer(memory, guest_address, sizeof(uint32_t), &source))
    {
      return false;
    }

    memcpy(out_value, source, sizeof(uint32_t));
    return true;
  }

  bool read_i32(const x64_emulator_memory& memory, uintptr_t guest_address, int32_t* out_value)
  {
    uint32_t word_value = 0;

    if (out_value == nullptr || !read_u32(memory, guest_address, &word_value))
    {
      return false;
    }

    *out_value = static_cast<int32_t>(word_value);
    return true;
  }

  bool read_u64(const x64_emulator_memory& memory, uintptr_t guest_address, uint64_t* out_value)
  {
    uint8_t* source = nullptr;

    if (out_value == nullptr || !translate_guest_pointer(memory, guest_address, sizeof(uint64_t), &source))
    {
      return false;
    }

    memcpy(out_value, source, sizeof(uint64_t));
    return true;
  }

  bool write_u64(const x64_emulator_memory& memory, uintptr_t guest_address, uint64_t value)
  {
    uint8_t* destination = nullptr;

    if (!translate_guest_pointer(memory, guest_address, sizeof(uint64_t), &destination))
    {
      return false;
    }

    memcpy(destination, &value, sizeof(uint64_t));
    return true;
  }

  void set_logic_flags(x64_emulator_state& state, uint64_t value, bool is_64_bit)
  {
    const uint64_t sign_mask = is_64_bit ? (1ULL << 63) : (1ULL << 31);
    const uint64_t masked_value = is_64_bit ? value : static_cast<uint32_t>(value);

    state.flags &= ~(RFLAGS_CARRY | RFLAGS_ZERO | RFLAGS_SIGN | RFLAGS_OVERFLOW);
    state.flags |= RFLAGS_RESERVED;

    if (masked_value == 0)
    {
      state.flags |= RFLAGS_ZERO;
    }

    if ((masked_value & sign_mask) != 0)
    {
      state.flags |= RFLAGS_SIGN;
    }
  }

  void set_compare_flags(x64_emulator_state& state, uint64_t lhs, uint64_t rhs, uint64_t result, bool is_64_bit)
  {
    const uint64_t sign_mask = is_64_bit ? (1ULL << 63) : (1ULL << 31);
    const uint64_t width_mask = is_64_bit ? UINT64_MAX : UINT32_MAX;
    const uint64_t masked_lhs = lhs & width_mask;
    const uint64_t masked_rhs = rhs & width_mask;
    const uint64_t masked_result = result & width_mask;

    state.flags &= ~(RFLAGS_CARRY | RFLAGS_ZERO | RFLAGS_SIGN | RFLAGS_OVERFLOW);
    state.flags |= RFLAGS_RESERVED;

    if (masked_result == 0)
    {
      state.flags |= RFLAGS_ZERO;
    }

    if ((masked_result & sign_mask) != 0)
    {
      state.flags |= RFLAGS_SIGN;
    }

    if (masked_lhs < masked_rhs)
    {
      state.flags |= RFLAGS_CARRY;
    }

    const bool lhs_sign = (masked_lhs & sign_mask) != 0;
    const bool rhs_sign = (masked_rhs & sign_mask) != 0;
    const bool result_sign = (masked_result & sign_mask) != 0;

    if (lhs_sign != rhs_sign && lhs_sign != result_sign)
    {
      state.flags |= RFLAGS_OVERFLOW;
    }
  }

  void set_add_flags(x64_emulator_state& state, uint64_t lhs, uint64_t rhs, uint64_t result, bool is_64_bit)
  {
    const uint64_t sign_mask = is_64_bit ? (1ULL << 63) : (1ULL << 31);
    const uint64_t width_mask = is_64_bit ? UINT64_MAX : UINT32_MAX;
    const uint64_t masked_lhs = lhs & width_mask;
    const uint64_t masked_rhs = rhs & width_mask;
    const uint64_t masked_result = result & width_mask;

    state.flags &= ~(RFLAGS_CARRY | RFLAGS_ZERO | RFLAGS_SIGN | RFLAGS_OVERFLOW);
    state.flags |= RFLAGS_RESERVED;

    if (masked_result == 0)
    {
      state.flags |= RFLAGS_ZERO;
    }

    if ((masked_result & sign_mask) != 0)
    {
      state.flags |= RFLAGS_SIGN;
    }

    if (masked_result < masked_lhs)
    {
      state.flags |= RFLAGS_CARRY;
    }

    const bool lhs_sign = (masked_lhs & sign_mask) != 0;
    const bool rhs_sign = (masked_rhs & sign_mask) != 0;
    const bool result_sign = (masked_result & sign_mask) != 0;

    if (lhs_sign == rhs_sign && lhs_sign != result_sign)
    {
      state.flags |= RFLAGS_OVERFLOW;
    }
  }

  bool push_u64(x64_emulator_state& state, const x64_emulator_memory& memory, uint64_t value)
  {
    uint64_t& stack_pointer = get_register64(state, static_cast<uint32_t>(x64_general_register::rsp));

    if (stack_pointer < memory.base_address + sizeof(uint64_t))
    {
      return false;
    }

    stack_pointer -= sizeof(uint64_t);
    return write_u64(memory, static_cast<uintptr_t>(stack_pointer), value);
  }

  bool pop_u64(x64_emulator_state& state, const x64_emulator_memory& memory, uint64_t* out_value)
  {
    uint64_t& stack_pointer = get_register64(state, static_cast<uint32_t>(x64_general_register::rsp));

    if (out_value == nullptr || !read_u64(memory, static_cast<uintptr_t>(stack_pointer), out_value))
    {
      return false;
    }

    stack_pointer += sizeof(uint64_t);
    return true;
  }

  bool run_x64_interpreter(
    x64_emulator_state& state,
    const x64_emulator_memory& memory,
    const x64_emulator_callbacks& callbacks,
    const x64_emulator_options& options,
    x64_emulator_result& result)
  {
    state.flags |= RFLAGS_RESERVED;
    result = {
      x64_emulator_completion::instruction_limit_reached,
      0,
      state.instruction_pointer,
      0,
    };

    if (callbacks.handle_syscall == nullptr || memory.bytes == nullptr || options.instruction_budget == 0)
    {
      result.completion = x64_emulator_completion::invalid_argument;
      return false;
    }

    while (result.retired_instructions < options.instruction_budget)
    {
      uintptr_t instruction_pointer = state.instruction_pointer;
      uint8_t opcode = 0;

      if (!read_u8(memory, instruction_pointer, &opcode))
      {
        result.completion = x64_emulator_completion::invalid_memory_access;
        result.fault_address = instruction_pointer;
        result.fault_opcode = 0;
        return true;
      }

      bool rex_w = false;

      if (opcode == 0x48)
      {
        rex_w = true;
        ++instruction_pointer;

        if (!read_u8(memory, instruction_pointer, &opcode))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          result.fault_address = instruction_pointer;
          result.fault_opcode = 0;
          return true;
        }
      }

      result.fault_address = state.instruction_pointer;
      result.fault_opcode = opcode;

      if (opcode >= 0x50 && opcode <= 0x57 && !rex_w)
      {
        const uint32_t register_index = opcode - 0x50;
        const uint64_t value = get_register64(state, register_index);

        if (!push_u64(state, memory, value))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        state.instruction_pointer = instruction_pointer + 1;
      }
      else if (opcode >= 0x58 && opcode <= 0x5F && !rex_w)
      {
        const uint32_t register_index = opcode - 0x58;
        uint64_t value = 0;

        if (!pop_u64(state, memory, &value))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        get_register64(state, register_index) = value;
        state.instruction_pointer = instruction_pointer + 1;
      }
      else if (opcode >= 0xB8 && opcode <= 0xBF && !rex_w)
      {
        const uint32_t register_index = opcode - 0xB8;
        uint32_t immediate = 0;

        if (!read_u32(memory, instruction_pointer + 1, &immediate))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        set_register32(state, register_index, immediate);
        state.instruction_pointer = instruction_pointer + 5;
      }
      else if (opcode == 0x8D && rex_w)
      {
        uint8_t modrm = 0;
        int32_t displacement = 0;

        if (
          !read_u8(memory, instruction_pointer + 1, &modrm)
          || !read_i32(memory, instruction_pointer + 2, &displacement))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
        const uint8_t register_index = static_cast<uint8_t>((modrm >> 3) & 0x7);
        const uint8_t rm = static_cast<uint8_t>(modrm & 0x7);

        if (mod != 0 || rm != 5)
        {
          result.completion = x64_emulator_completion::unsupported_instruction;
          return true;
        }

        const uintptr_t next_instruction = instruction_pointer + 6;
        get_register64(state, register_index) = next_instruction + displacement;
        state.instruction_pointer = next_instruction;
      }
      else if (opcode == 0x31 && !rex_w)
      {
        uint8_t modrm = 0;

        if (!read_u8(memory, instruction_pointer + 1, &modrm))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
        const uint8_t source_register = static_cast<uint8_t>((modrm >> 3) & 0x7);
        const uint8_t destination_register = static_cast<uint8_t>(modrm & 0x7);

        if (mod != 3)
        {
          result.completion = x64_emulator_completion::unsupported_instruction;
          return true;
        }

        const uint32_t result_value
          = get_register32(state, destination_register) ^ get_register32(state, source_register);
        set_register32(state, destination_register, result_value);
        set_logic_flags(state, result_value, false);
        state.instruction_pointer = instruction_pointer + 2;
      }
      else if (opcode == 0x83)
      {
        uint8_t modrm = 0;
        int8_t immediate = 0;

        if (!read_u8(memory, instruction_pointer + 1, &modrm) || !read_i8(memory, instruction_pointer + 2, &immediate))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        const uint8_t mod = static_cast<uint8_t>((modrm >> 6) & 0x3);
        const uint8_t operation = static_cast<uint8_t>((modrm >> 3) & 0x7);
        const uint8_t register_index = static_cast<uint8_t>(modrm & 0x7);

        if (mod != 3)
        {
          result.completion = x64_emulator_completion::unsupported_instruction;
          return true;
        }

        if (rex_w)
        {
          const uint64_t lhs = get_register64(state, register_index);
          const uint64_t rhs = static_cast<uint64_t>(static_cast<int64_t>(immediate));

          if (operation == 0)
          {
            const uint64_t sum = lhs + rhs;
            get_register64(state, register_index) = sum;
            set_add_flags(state, lhs, rhs, sum, true);
          }
          else if (operation == 5)
          {
            const uint64_t difference = lhs - rhs;
            get_register64(state, register_index) = difference;
            set_compare_flags(state, lhs, rhs, difference, true);
          }
          else if (operation == 7)
          {
            const uint64_t difference = lhs - rhs;
            set_compare_flags(state, lhs, rhs, difference, true);
          }
          else
          {
            result.completion = x64_emulator_completion::unsupported_instruction;
            return true;
          }
        }
        else
        {
          const uint32_t lhs = get_register32(state, register_index);
          const uint32_t rhs = static_cast<uint32_t>(static_cast<int32_t>(immediate));

          if (operation == 0)
          {
            const uint32_t sum = lhs + rhs;
            set_register32(state, register_index, sum);
            set_add_flags(state, lhs, rhs, sum, false);
          }
          else if (operation == 5)
          {
            const uint32_t difference = lhs - rhs;
            set_register32(state, register_index, difference);
            set_compare_flags(state, lhs, rhs, difference, false);
          }
          else if (operation == 7)
          {
            const uint32_t difference = lhs - rhs;
            set_compare_flags(state, lhs, rhs, difference, false);
          }
          else
          {
            result.completion = x64_emulator_completion::unsupported_instruction;
            return true;
          }
        }

        state.instruction_pointer = instruction_pointer + 3;
      }
      else if (opcode == 0x90 && !rex_w)
      {
        state.instruction_pointer = instruction_pointer + 1;
      }
      else if (opcode == 0x0F && !rex_w)
      {
        uint8_t secondary_opcode = 0;

        if (!read_u8(memory, instruction_pointer + 1, &secondary_opcode))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        if (secondary_opcode != 0x05)
        {
          result.completion = x64_emulator_completion::unsupported_instruction;
          result.fault_opcode = secondary_opcode;
          return true;
        }

        bool should_continue = false;
        state.instruction_pointer = instruction_pointer + 2;
        const int32_t syscall_result = callbacks.handle_syscall(callbacks.context, state, &should_continue);
        get_register64(state, static_cast<uint32_t>(x64_general_register::rax))
          = static_cast<uint64_t>(static_cast<int64_t>(syscall_result));

        if (!should_continue)
        {
          result.completion = x64_emulator_completion::thread_exited;
          ++result.retired_instructions;
          return true;
        }
      }
      else if (opcode == 0xC3 && !rex_w)
      {
        uint64_t return_address = 0;

        if (!pop_u64(state, memory, &return_address))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        state.instruction_pointer = static_cast<uintptr_t>(return_address);
      }
      else if (opcode == 0xE8 && !rex_w)
      {
        int32_t displacement = 0;

        if (!read_i32(memory, instruction_pointer + 1, &displacement))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        const uintptr_t return_address = instruction_pointer + 5;

        if (!push_u64(state, memory, return_address))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        state.instruction_pointer = return_address + displacement;
      }
      else if (opcode == 0xE9 && !rex_w)
      {
        int32_t displacement = 0;

        if (!read_i32(memory, instruction_pointer + 1, &displacement))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        state.instruction_pointer = instruction_pointer + 5 + displacement;
      }
      else if (opcode == 0xEB && !rex_w)
      {
        int8_t displacement = 0;

        if (!read_i8(memory, instruction_pointer + 1, &displacement))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        state.instruction_pointer = instruction_pointer + 2 + displacement;
      }
      else if ((opcode == 0x74 || opcode == 0x75) && !rex_w)
      {
        int8_t displacement = 0;

        if (!read_i8(memory, instruction_pointer + 1, &displacement))
        {
          result.completion = x64_emulator_completion::invalid_memory_access;
          return true;
        }

        const bool zero_flag_set = (state.flags & RFLAGS_ZERO) != 0;
        const bool take_branch = opcode == 0x74 ? zero_flag_set : !zero_flag_set;
        state.instruction_pointer = take_branch ? instruction_pointer + 2 + displacement : instruction_pointer + 2;
      }
      else
      {
        result.completion = x64_emulator_completion::unsupported_instruction;
        return true;
      }

      ++result.retired_instructions;
    }

    result.completion = x64_emulator_completion::instruction_limit_reached;
    return true;
  }
}

bool run_x64_emulator(
  x64_emulator_state& state,
  const x64_emulator_memory& memory,
  const x64_emulator_callbacks& callbacks,
  const x64_emulator_options& options,
  x64_emulator_result* out_result)
{
  if (out_result == nullptr)
  {
    return false;
  }

  if (options.engine != x64_emulator_engine::interpreter)
  {
    *out_result = {
      x64_emulator_completion::unsupported_engine,
      0,
      state.instruction_pointer,
      0,
    };
    return true;
  }

  return run_x64_interpreter(state, memory, callbacks, options, *out_result);
}

const char* describe_x64_emulator_completion(x64_emulator_completion completion)
{
  switch (completion)
  {
  case x64_emulator_completion::thread_exited:
    return "x64 emulator thread exited cleanly";
  case x64_emulator_completion::instruction_limit_reached:
    return "x64 emulator hit the instruction budget";
  case x64_emulator_completion::invalid_memory_access:
    return "x64 emulator touched unmapped guest memory";
  case x64_emulator_completion::unsupported_instruction:
    return "x64 emulator hit an unsupported instruction";
  case x64_emulator_completion::invalid_argument:
    return "x64 emulator received an invalid argument";
  case x64_emulator_completion::unsupported_engine:
    return "x64 emulator backend is not implemented";
  }

  return "x64 emulator failed with an unknown completion state";
}
