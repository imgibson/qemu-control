cmake_minimum_required(VERSION 3.8)

project(qemu-control)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSONS OFF)

# Source
add_executable(qemu-control WIN32
  source/qemu-control.cpp
  resource/resource.h
  resource/resource.rc
)

# Include
target_include_directories(qemu-control PRIVATE
  resource
)
