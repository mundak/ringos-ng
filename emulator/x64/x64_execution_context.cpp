#include "x64_execution_context.h"

#include "memory.h"

namespace
{
  constexpr uint64_t RFLAGS_RESERVED = 1ULL << 1;
  constexpr uint64_t RFLAGS_CARRY = 1ULL << 0;
  constexpr uint64_t RFLAGS_ZERO = 1ULL << 6;
  constexpr uint64_t RFLAGS_SIGN = 1ULL << 7;
  constexpr uint64_t RFLAGS_OVERFLOW = 1ULL << 11;
}

x64_execution_context::x64_execution_context(
  x64_emulator_state& state,
  const x64_emulator_memory& memory,
  const x64_emulator_callbacks& callbacks,
  const x64_emulator_options& options,
  x64_emulator_result& result)
  : m_state(state)
  , m_memory(memory)
  , m_callbacks(callbacks)
  , m_options(options)
  , m_result(result)
{
}

void x64_execution_context::initialize_result()
{
  m_state.flags |= RFLAGS_RESERVED;
  m_result = {
    X64_EMULATOR_COMPLETION_INSTRUCTION_LIMIT_REACHED,
    0,
    m_state.instruction_pointer,
    0,
  };
}

bool x64_execution_context::is_valid_invocation() const
{
  return m_callbacks.handle_syscall != nullptr && m_memory.bytes != nullptr && m_options.instruction_budget != 0;
}

x64_emulator_state& x64_execution_context::get_state()
{
  return m_state;
}

const x64_emulator_state& x64_execution_context::get_state() const
{
  return m_state;
}

const x64_emulator_callbacks& x64_execution_context::get_callbacks() const
{
  return m_callbacks;
}

const x64_emulator_options& x64_execution_context::get_options() const
{
  return m_options;
}

x64_emulator_result& x64_execution_context::get_result()
{
  return m_result;
}

const x64_emulator_result& x64_execution_context::get_result() const
{
  return m_result;
}

void x64_execution_context::set_fault(const x64_decoded_instruction& instruction)
{
  m_result.fault_address = instruction.address;
  m_result.fault_opcode = instruction.opcode;
}

void x64_execution_context::set_invalid_memory_access(uintptr_t fault_address, uint8_t fault_opcode)
{
  m_result.completion = X64_EMULATOR_COMPLETION_INVALID_MEMORY_ACCESS;
  m_result.fault_address = fault_address;
  m_result.fault_opcode = fault_opcode;
}

void x64_execution_context::set_unsupported_instruction(uint8_t fault_opcode)
{
  m_result.completion = X64_EMULATOR_COMPLETION_UNSUPPORTED_INSTRUCTION;
  m_result.fault_opcode = fault_opcode;
}

uint64_t& x64_execution_context::get_register64(uint32_t register_index)
{
  return m_state.general_registers[register_index];
}

uint32_t x64_execution_context::get_register32(uint32_t register_index) const
{
  return static_cast<uint32_t>(m_state.general_registers[register_index]);
}

uint8_t x64_execution_context::get_register8_low(uint32_t register_index) const
{
  return static_cast<uint8_t>(m_state.general_registers[register_index] & 0xFFU);
}

void x64_execution_context::set_register8_low(uint32_t register_index, uint8_t value)
{
  m_state.general_registers[register_index] = (m_state.general_registers[register_index] & ~0xFFULL) | value;
}

x64_simd_register& x64_execution_context::get_simd_register(uint32_t register_index)
{
  return m_state.simd_registers[register_index];
}

const x64_simd_register& x64_execution_context::get_simd_register(uint32_t register_index) const
{
  return m_state.simd_registers[register_index];
}

void x64_execution_context::set_register32(uint32_t register_index, uint32_t value)
{
  m_state.general_registers[register_index] = value;
}

bool x64_execution_context::read_u8(uintptr_t guest_address, uint8_t* out_value) const
{
  uint8_t* source = nullptr;

  if (out_value == nullptr || !translate_guest_pointer(guest_address, sizeof(uint8_t), &source))
  {
    return false;
  }

  *out_value = *source;
  return true;
}

bool x64_execution_context::read_i8(uintptr_t guest_address, int8_t* out_value) const
{
  uint8_t byte_value = 0;

  if (out_value == nullptr || !read_u8(guest_address, &byte_value))
  {
    return false;
  }

  *out_value = static_cast<int8_t>(byte_value);
  return true;
}

bool x64_execution_context::read_u32(uintptr_t guest_address, uint32_t* out_value) const
{
  uint8_t* source = nullptr;

  if (out_value == nullptr || !translate_guest_pointer(guest_address, sizeof(uint32_t), &source))
  {
    return false;
  }

  memcpy(out_value, source, sizeof(uint32_t));
  return true;
}

bool x64_execution_context::read_i32(uintptr_t guest_address, int32_t* out_value) const
{
  uint32_t word_value = 0;

  if (out_value == nullptr || !read_u32(guest_address, &word_value))
  {
    return false;
  }

  *out_value = static_cast<int32_t>(word_value);
  return true;
}

bool x64_execution_context::read_u64(uintptr_t guest_address, uint64_t* out_value) const
{
  uint8_t* source = nullptr;

  if (out_value == nullptr || !translate_guest_pointer(guest_address, sizeof(uint64_t), &source))
  {
    return false;
  }

  memcpy(out_value, source, sizeof(uint64_t));
  return true;
}

bool x64_execution_context::read_u128(uintptr_t guest_address, x64_simd_register* out_value) const
{
  uint8_t* source = nullptr;

  if (out_value == nullptr || !translate_guest_pointer(guest_address, sizeof(out_value->bytes), &source))
  {
    return false;
  }

  memcpy(out_value->bytes, source, sizeof(out_value->bytes));
  return true;
}

bool x64_execution_context::write_u8(uintptr_t guest_address, uint8_t value) const
{
  uint8_t* destination = nullptr;

  if (!translate_guest_pointer(guest_address, sizeof(uint8_t), &destination))
  {
    return false;
  }

  *destination = value;
  return true;
}

bool x64_execution_context::write_u32(uintptr_t guest_address, uint32_t value) const
{
  uint8_t* destination = nullptr;

  if (!translate_guest_pointer(guest_address, sizeof(uint32_t), &destination))
  {
    return false;
  }

  memcpy(destination, &value, sizeof(uint32_t));
  return true;
}

bool x64_execution_context::write_u64(uintptr_t guest_address, uint64_t value) const
{
  uint8_t* destination = nullptr;

  if (!translate_guest_pointer(guest_address, sizeof(uint64_t), &destination))
  {
    return false;
  }

  memcpy(destination, &value, sizeof(uint64_t));
  return true;
}

bool x64_execution_context::write_u128(uintptr_t guest_address, const x64_simd_register& value) const
{
  uint8_t* destination = nullptr;

  if (!translate_guest_pointer(guest_address, sizeof(value.bytes), &destination))
  {
    return false;
  }

  memcpy(destination, value.bytes, sizeof(value.bytes));
  return true;
}

void x64_execution_context::set_logic_flags(uint64_t value, bool is_64_bit)
{
  const uint64_t sign_mask = is_64_bit ? (1ULL << 63) : (1ULL << 31);
  const uint64_t masked_value = is_64_bit ? value : static_cast<uint32_t>(value);

  m_state.flags &= ~(RFLAGS_CARRY | RFLAGS_ZERO | RFLAGS_SIGN | RFLAGS_OVERFLOW);
  m_state.flags |= RFLAGS_RESERVED;

  if (masked_value == 0)
  {
    m_state.flags |= RFLAGS_ZERO;
  }

  if ((masked_value & sign_mask) != 0)
  {
    m_state.flags |= RFLAGS_SIGN;
  }
}

void x64_execution_context::set_compare_flags(uint64_t lhs, uint64_t rhs, uint64_t result, bool is_64_bit)
{
  const uint64_t sign_mask = is_64_bit ? (1ULL << 63) : (1ULL << 31);
  const uint64_t width_mask = is_64_bit ? UINT64_MAX : UINT32_MAX;
  const uint64_t masked_lhs = lhs & width_mask;
  const uint64_t masked_rhs = rhs & width_mask;
  const uint64_t masked_result = result & width_mask;

  m_state.flags &= ~(RFLAGS_CARRY | RFLAGS_ZERO | RFLAGS_SIGN | RFLAGS_OVERFLOW);
  m_state.flags |= RFLAGS_RESERVED;

  if (masked_result == 0)
  {
    m_state.flags |= RFLAGS_ZERO;
  }

  if ((masked_result & sign_mask) != 0)
  {
    m_state.flags |= RFLAGS_SIGN;
  }

  if (masked_lhs < masked_rhs)
  {
    m_state.flags |= RFLAGS_CARRY;
  }

  const bool lhs_sign = (masked_lhs & sign_mask) != 0;
  const bool rhs_sign = (masked_rhs & sign_mask) != 0;
  const bool result_sign = (masked_result & sign_mask) != 0;

  if (lhs_sign != rhs_sign && lhs_sign != result_sign)
  {
    m_state.flags |= RFLAGS_OVERFLOW;
  }
}

void x64_execution_context::set_add_flags(uint64_t lhs, uint64_t rhs, uint64_t result, bool is_64_bit)
{
  const uint64_t sign_mask = is_64_bit ? (1ULL << 63) : (1ULL << 31);
  const uint64_t width_mask = is_64_bit ? UINT64_MAX : UINT32_MAX;
  const uint64_t masked_lhs = lhs & width_mask;
  const uint64_t masked_rhs = rhs & width_mask;
  const uint64_t masked_result = result & width_mask;

  m_state.flags &= ~(RFLAGS_CARRY | RFLAGS_ZERO | RFLAGS_SIGN | RFLAGS_OVERFLOW);
  m_state.flags |= RFLAGS_RESERVED;

  if (masked_result == 0)
  {
    m_state.flags |= RFLAGS_ZERO;
  }

  if ((masked_result & sign_mask) != 0)
  {
    m_state.flags |= RFLAGS_SIGN;
  }

  if (masked_result < masked_lhs)
  {
    m_state.flags |= RFLAGS_CARRY;
  }

  const bool lhs_sign = (masked_lhs & sign_mask) != 0;
  const bool rhs_sign = (masked_rhs & sign_mask) != 0;
  const bool result_sign = (masked_result & sign_mask) != 0;

  if (lhs_sign == rhs_sign && lhs_sign != result_sign)
  {
    m_state.flags |= RFLAGS_OVERFLOW;
  }
}

bool x64_execution_context::push_u64(uint64_t value)
{
  uint64_t& stack_pointer = get_register64(static_cast<uint32_t>(X64_GENERAL_REGISTER_RSP));

  if (stack_pointer < m_memory.base_address + sizeof(uint64_t))
  {
    return false;
  }

  stack_pointer -= sizeof(uint64_t);
  return write_u64(static_cast<uintptr_t>(stack_pointer), value);
}

bool x64_execution_context::pop_u64(uint64_t* out_value)
{
  uint64_t& stack_pointer = get_register64(static_cast<uint32_t>(X64_GENERAL_REGISTER_RSP));

  if (out_value == nullptr || !read_u64(static_cast<uintptr_t>(stack_pointer), out_value))
  {
    return false;
  }

  stack_pointer += sizeof(uint64_t);
  return true;
}

bool x64_execution_context::translate_guest_pointer(uintptr_t guest_address, size_t length, uint8_t** out_pointer) const
{
  if (out_pointer == nullptr || m_memory.bytes == nullptr)
  {
    return false;
  }

  if (guest_address < m_memory.base_address)
  {
    return false;
  }

  const uintptr_t guest_offset = guest_address - m_memory.base_address;

  if (guest_offset > m_memory.size)
  {
    return false;
  }

  if (length > m_memory.size - static_cast<size_t>(guest_offset))
  {
    return false;
  }

  *out_pointer = m_memory.bytes + guest_offset;
  return true;
}
