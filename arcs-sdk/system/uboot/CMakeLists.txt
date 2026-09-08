cmake_minimum_required(VERSION 3.19)

# This is always a bare-metal cross build. Set the target system before the
# first project() call so CMake does not inject host linker flags on macOS.
set(CMAKE_SYSTEM_NAME Generic)

project(boot NONE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/boot_standalone.cmake)
