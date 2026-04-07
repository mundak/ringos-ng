cmake_minimum_required(VERSION 3.27)

project(ringos_hello_world_cpp LANGUAGES CXX)

if(NOT DEFINED RINGOS_TARGET_TRIPLE)
  message(FATAL_ERROR
    "Configure this sample with the published ringos installed-toolchain bundle. "
    "Pass -DCMAKE_TOOLCHAIN_FILE=<bundle>/cmake/ringos-toolchain.cmake "
    "and -DRINGOS_TARGET_ARCH=<x64|arm64>."
  )
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hello_world_cpp
  hello_world.cpp
)

set_target_properties(hello_world_cpp
  PROPERTIES
    SUFFIX ".exe"
)

target_compile_definitions(hello_world_cpp
  PRIVATE
    _LIBCPP_OBJECT_FORMAT_COFF
)

target_compile_options(hello_world_cpp
  PRIVATE
    -fno-ms-compatibility
    -O2
    -Wall
    -Wextra
    -Wpedantic
)
