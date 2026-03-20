set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_ASM_COMPILER gcc)
set(CMAKE_OBJCOPY objcopy)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_INIT "-m64 -march=x86-64 -ffreestanding -nostdlib -nostartfiles")
set(CMAKE_CXX_FLAGS_INIT "-m64 -march=x86-64 -ffreestanding -nostdlib -nostartfiles -fno-exceptions -fno-rtti")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib")
