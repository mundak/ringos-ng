set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_OBJCOPY aarch64-linux-gnu-objcopy)

# Avoid linking during CMake compiler detection
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Bare-metal flags applied globally
add_compile_options(-ffreestanding -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic)
add_link_options(-nostdlib -nostartfiles)
