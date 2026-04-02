include(${CMAKE_CURRENT_LIST_DIR}/ringos-llvm-root.cmake)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

ringos_resolve_llvm_root(ringos_arm64_llvm_root)
set(RINGOS_LLVM_TOOLCHAIN_ROOT ${ringos_arm64_llvm_root})
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
	RINGOS_PREVIOUS_STAGE_TOOLCHAIN_ROOT
	RINGOS_LLVM_TOOLCHAIN_ROOT)

ringos_find_llvm_tool(clang RINGOS_CLANG clang clang-18 clang-17)
ringos_find_llvm_tool(clang++ RINGOS_CLANGXX clang++ clang++-18 clang++-17)
ringos_find_llvm_tool(llvm-objcopy RINGOS_LLVM_OBJCOPY llvm-objcopy llvm-objcopy-18 llvm-objcopy-17)
ringos_find_llvm_tool(ld.lld RINGOS_LLD ld.lld ld.lld-18 ld.lld-17)

set(CMAKE_C_COMPILER ${RINGOS_CLANG})
set(CMAKE_CXX_COMPILER ${RINGOS_CLANGXX})
set(CMAKE_ASM_COMPILER ${RINGOS_CLANG})

set(CMAKE_C_COMPILER_TARGET aarch64-unknown-ringos)
set(CMAKE_CXX_COMPILER_TARGET aarch64-unknown-ringos)
set(CMAKE_ASM_COMPILER_TARGET aarch64-unknown-ringos)

# Avoid linking during CMake compiler detection
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_OBJCOPY ${RINGOS_LLVM_OBJCOPY})

# Invoke lld directly for a consistent, compiler-driver-independent link step.
# -nostdlib/-nostartfiles are compiler-driver flags that prevent libc and CRT
# objects from being linked automatically. When invoking lld directly, those
# defaults are never applied, so the flags are not needed.
set(CMAKE_C_LINK_EXECUTABLE "\"${RINGOS_LLD}\" <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_EXECUTABLE "\"${RINGOS_LLD}\" <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_ASM_LINK_EXECUTABLE "\"${RINGOS_LLD}\" <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

# Bare-metal flags applied globally
add_compile_options(-ffreestanding -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic)
