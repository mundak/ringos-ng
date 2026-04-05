include(${CMAKE_CURRENT_LIST_DIR}/ringos-llvm-root.cmake)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

ringos_resolve_llvm_root(ringos_x64_llvm_root)
if(NOT DEFINED RINGOS_TOOLCHAIN_ROOT OR RINGOS_TOOLCHAIN_ROOT STREQUAL "")
  set(RINGOS_TOOLCHAIN_ROOT ${ringos_x64_llvm_root})
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
        RINGOS_ACTIVE_LLVM_ROOT
        RINGOS_TOOLCHAIN_ROOT)

ringos_find_llvm_tool(clang RINGOS_CLANG clang clang-18 clang-17)
ringos_find_llvm_tool(clang++ RINGOS_CLANGXX clang++ clang++-18 clang++-17)
ringos_find_llvm_tool(llvm-objcopy RINGOS_LLVM_OBJCOPY llvm-objcopy llvm-objcopy-18 llvm-objcopy-17)
ringos_find_llvm_tool(ld.lld RINGOS_LLD ld.lld ld.lld-18 ld.lld-17)

set(CMAKE_C_COMPILER ${RINGOS_CLANG})
set(CMAKE_CXX_COMPILER ${RINGOS_CLANGXX})
set(CMAKE_ASM_COMPILER ${RINGOS_CLANG})

set(CMAKE_C_COMPILER_TARGET x86_64-unknown-ringos-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-unknown-ringos-msvc)
set(CMAKE_ASM_COMPILER_TARGET x86_64-unknown-ringos-msvc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_OBJCOPY ${RINGOS_LLVM_OBJCOPY})

set(CMAKE_C_LINK_EXECUTABLE "\"${RINGOS_LLD}\" <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_EXECUTABLE "\"${RINGOS_LLD}\" <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_ASM_LINK_EXECUTABLE "\"${RINGOS_LLD}\" <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

add_compile_options(-ffreestanding -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic)
