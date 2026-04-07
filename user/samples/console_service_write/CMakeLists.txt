cmake_minimum_required(VERSION 3.27)

project(ringos_console_service_write LANGUAGES C)

if(NOT DEFINED RINGOS_TARGET_TRIPLE)
  message(FATAL_ERROR
    "Configure this sample with the published ringos installed-toolchain bundle. "
    "Pass -DCMAKE_TOOLCHAIN_FILE=<bundle>/cmake/ringos-toolchain.cmake "
    "and -DRINGOS_TARGET_ARCH=<x64|arm64>."
  )
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(console_service_write
  console_service_write.c
)

target_compile_options(console_service_write
  PRIVATE
    -O2
    -Wall
    -Wextra
    -Wpedantic
)
