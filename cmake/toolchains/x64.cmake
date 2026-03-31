set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)

set(CMAKE_C_COMPILER_TARGET x86_64-unknown-none-elf)
set(CMAKE_CXX_COMPILER_TARGET x86_64-unknown-none-elf)
set(CMAKE_ASM_COMPILER_TARGET x86_64-unknown-none-elf)

# Avoid linking during CMake compiler detection
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Locate LLVM tools — try unversioned first, then versioned fallbacks
find_program(CMAKE_OBJCOPY NAMES llvm-objcopy llvm-objcopy-18 llvm-objcopy-17 REQUIRED)
find_program(RINGOS_LLD NAMES ld.lld ld.lld-18 ld.lld-17 REQUIRED)

# For x86_64-unknown-none-elf, clang delegates linking to g++ rather than lld.
# Invoke lld directly to avoid this limitation.
# -nostdlib/-nostartfiles are compiler-driver flags that prevent libc and CRT
# objects from being linked automatically. When invoking lld directly, those
# defaults are never applied, so the flags are not needed.
set(CMAKE_C_LINK_EXECUTABLE "\"${RINGOS_LLD}\" <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_EXECUTABLE "\"${RINGOS_LLD}\" <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_ASM_LINK_EXECUTABLE "\"${RINGOS_LLD}\" <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

# Bare-metal flags applied globally
add_compile_options(-ffreestanding -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic)
