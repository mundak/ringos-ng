#pragma once

#include "user_runtime.h"

#include <stddef.h>
#include <stdint.h>

struct x64_emulator_state;

constexpr size_t ARM64_USER_RUNTIME_PAGE_SIZE = 4096;
constexpr size_t ARM64_USER_RUNTIME_LARGE_PAGE_SIZE = 0x200000;
constexpr size_t ARM64_USER_RUNTIME_USER_REGION_SIZE = ARM64_USER_RUNTIME_LARGE_PAGE_SIZE;

struct alignas(ARM64_USER_RUNTIME_PAGE_SIZE) arm64_translation_table
{
  uint64_t entries[512];
};

struct arm64_process_storage
{
  arm64_translation_table root_table;
  arm64_translation_table lower_block_table;
  arm64_translation_table kernel_block_table;
  alignas(ARM64_USER_RUNTIME_PAGE_SIZE) uint8_t
    user_region_storage[ARM64_USER_RUNTIME_USER_REGION_SIZE + ARM64_USER_RUNTIME_LARGE_PAGE_SIZE];
};

enum arm64_user_image_kind : uint32_t
{
  ARM64_USER_IMAGE_KIND_UNKNOWN = 0,
  ARM64_USER_IMAGE_KIND_NATIVE_ARM64_PE64 = 1,
  ARM64_USER_IMAGE_KIND_X64_PE64 = 2,
};

class arm64_initial_user_runtime_platform final
{
public:
  void initialize(initial_user_runtime_bootstrap& bootstrap);
  void prepare_thread_launch(const process& initial_process, const thread& initial_thread);
  [[noreturn]] void enter_user_thread(const process& initial_process, const thread& initial_thread);
  void activate_process_address_space(const process* process_context);
  arm64_user_image_kind get_user_image_kind() const;

private:
  static int32_t dispatch_x64_syscall(void* context, const x64_emulator_state& state, bool* out_should_continue);
  uintptr_t initialize_arm64_pe_image(const uint8_t* image_bytes, size_t image_size, arm64_process_storage& storage);
  uintptr_t initialize_x64_emulator_image(
    const uint8_t* image_bytes, size_t image_size, arm64_process_storage& storage);
  void enable_fp_simd();
  void enable_mmu();
  void initialize_process_storage(arm64_process_storage& storage);
  void initialize_translation_tables(arm64_process_storage& storage);
  void invalidate_tlb();
  void populate_native_bootstrap_for_process(
    arm64_process_storage& storage,
    const uint8_t* image_start,
    const uint8_t* image_end,
    initial_process_configuration& process_configuration);
  void populate_x64_emulator_bootstrap(
    arm64_process_storage& storage,
    const uint8_t* image_start,
    const uint8_t* image_end,
    initial_process_configuration& process_configuration);
  uint64_t read_system_control() const;
  void write_mair(uintptr_t value);
  void write_system_control(uint64_t value);
  void write_tcr(uint64_t value);
  void write_ttbr0(uintptr_t value);
  void write_vector_base(uintptr_t vector_base);

  arm64_process_storage m_process_storage[USER_RUNTIME_MAX_INITIAL_PROCESSES] {};
  arm64_user_image_kind m_user_image_kind = ARM64_USER_IMAGE_KIND_UNKNOWN;
  bool m_mmu_enabled = false;
};

arm64_initial_user_runtime_platform& get_arm64_initial_user_runtime_platform();
void initialize_arm64_platform(void* context, initial_user_runtime_bootstrap& bootstrap);
void prepare_arm64_platform(void* context, const process& initial_process, const thread& initial_thread);
[[noreturn]] void enter_arm64_platform(void* context, const process& initial_process, const thread& initial_thread);
