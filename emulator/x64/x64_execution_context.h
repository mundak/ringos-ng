#pragma once

#include "x64_emulator.h"
#include "x64_instruction.h"

class x64_execution_context
{
public:
  x64_execution_context(
    x64_emulator_state& state,
    const x64_emulator_memory& memory,
    const x64_emulator_callbacks& callbacks,
    const x64_emulator_options& options,
    x64_emulator_result& result);

  void initialize_result();
  bool is_valid_invocation() const;

  x64_emulator_state& get_state();
  const x64_emulator_state& get_state() const;
  const x64_emulator_callbacks& get_callbacks() const;
  const x64_emulator_options& get_options() const;
  x64_emulator_result& get_result();
  const x64_emulator_result& get_result() const;

  void set_fault(const x64_decoded_instruction& instruction);
  void set_invalid_memory_access(uintptr_t fault_address, uint8_t fault_opcode);
  void set_unsupported_instruction(uint8_t fault_opcode);

  uint64_t& get_register64(uint32_t register_index);
  uint32_t get_register32(uint32_t register_index) const;
  void set_register32(uint32_t register_index, uint32_t value);

  bool read_u8(uintptr_t guest_address, uint8_t* out_value) const;
  bool read_i8(uintptr_t guest_address, int8_t* out_value) const;
  bool read_u32(uintptr_t guest_address, uint32_t* out_value) const;
  bool read_i32(uintptr_t guest_address, int32_t* out_value) const;
  bool read_u64(uintptr_t guest_address, uint64_t* out_value) const;
  bool write_u32(uintptr_t guest_address, uint32_t value) const;
  bool write_u64(uintptr_t guest_address, uint64_t value) const;

  void set_logic_flags(uint64_t value, bool is_64_bit);
  void set_compare_flags(uint64_t lhs, uint64_t rhs, uint64_t result, bool is_64_bit);
  void set_add_flags(uint64_t lhs, uint64_t rhs, uint64_t result, bool is_64_bit);

  bool push_u64(uint64_t value);
  bool pop_u64(uint64_t* out_value);

private:
  bool translate_guest_pointer(uintptr_t guest_address, size_t length, uint8_t** out_pointer) const;

  x64_emulator_state& m_state;
  const x64_emulator_memory& m_memory;
  const x64_emulator_callbacks& m_callbacks;
  const x64_emulator_options& m_options;
  x64_emulator_result& m_result;
};
