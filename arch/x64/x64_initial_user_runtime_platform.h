#pragma once

#include "user_runtime.h"
#include "x64_pe64_image.h"

#include <stddef.h>
#include <stdint.h>

inline constexpr size_t X64_LOW_IDENTITY_SIZE = 0x400000;
inline constexpr size_t X64_LOW_PAGE_TABLE_COUNT = X64_LOW_IDENTITY_SIZE / 0x200000;

inline constexpr uint32_t X64_PRESERVED_REGISTER_RSI_INDEX = 0;
inline constexpr uint32_t X64_PRESERVED_REGISTER_RDI_INDEX = 1;
inline constexpr uint32_t X64_PRESERVED_REGISTER_RBX_INDEX = 2;
inline constexpr uint32_t X64_PRESERVED_REGISTER_RBP_INDEX = 3;
inline constexpr uint32_t X64_PRESERVED_REGISTER_R12_INDEX = 4;
inline constexpr uint32_t X64_PRESERVED_REGISTER_R13_INDEX = 5;
inline constexpr uint32_t X64_PRESERVED_REGISTER_R14_INDEX = 6;
inline constexpr uint32_t X64_PRESERVED_REGISTER_R15_INDEX = 7;

struct alignas(X64_USER_IMAGE_PAGE_SIZE) x64_page_table
{
  uint64_t entries[512];
};

struct alignas(16) x64_cpu_local
{
  uint64_t user_stack_pointer;
  uint64_t kernel_stack_pointer;
};

struct x64_process_storage
{
  x64_page_table pml4;
  x64_page_table pdpt;
  x64_page_table page_directory;
  x64_page_table low_page_tables[X64_LOW_PAGE_TABLE_COUNT];
  x64_page_table user_page_table;
  alignas(X64_USER_IMAGE_PAGE_SIZE) uint8_t user_image_pages[X64_USER_IMAGE_PAGE_COUNT][X64_USER_IMAGE_PAGE_SIZE];
  alignas(X64_USER_IMAGE_PAGE_SIZE) uint8_t user_stack_page[X64_USER_IMAGE_PAGE_SIZE];
  alignas(X64_USER_IMAGE_PAGE_SIZE) uint8_t user_rpc_transfer_page[X64_USER_IMAGE_PAGE_SIZE];
};

class x64_initial_user_runtime_platform final
{
public:
  void initialize(initial_user_runtime_bootstrap& bootstrap);
  void prepare_thread_launch(const process& initial_process, const thread& initial_thread);
  [[noreturn]] void enter_user_thread(const process& initial_process, const thread& initial_thread);
  void prepare_user_thread(const thread* current_thread);

private:
  void initialize_low_identity_mappings(x64_process_storage& storage);
  void initialize_user_region(x64_process_storage& storage);
  uintptr_t initialize_user_image(x64_process_storage& storage, const uint8_t* image_start, const uint8_t* image_end);
  void initialize_process_storage(x64_process_storage& storage);
  void initialize_syscall_msrs();
  void populate_bootstrap_for_process(
    x64_process_storage& storage,
    const uint8_t* image_start,
    const uint8_t* image_end,
    address_space& address_space_info,
    thread_context& thread_context_info);

  x64_process_storage m_process_storage[USER_RUNTIME_MAX_INITIAL_PROCESSES] {};
  x64_cpu_local m_cpu_local {};
};

x64_initial_user_runtime_platform& get_x64_initial_user_runtime_platform();
void initialize_x64_platform(void* context, initial_user_runtime_bootstrap& bootstrap);
void prepare_x64_platform(void* context, const process& initial_process, const thread& initial_thread);
[[noreturn]] void enter_x64_platform(void* context, const process& initial_process, const thread& initial_thread);
