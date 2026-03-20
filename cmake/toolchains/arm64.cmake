set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_OBJCOPY aarch64-linux-gnu-objcopy)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_INIT "-ffreestanding -nostdlib -nostartfiles")
set(CMAKE_CXX_FLAGS_INIT "-ffreestanding -nostdlib -nostartfiles -fno-exceptions -fno-rtti")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib")
