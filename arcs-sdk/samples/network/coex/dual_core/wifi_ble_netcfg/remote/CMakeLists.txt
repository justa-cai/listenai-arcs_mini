cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(ap)

listenai_include_directories(
    ./
)

# 由 sample 顶层 CMake 透传：CP 镜像 flash 基地址，main.c 中做 _Static_assert
if(DEFINED EXPECTED_CP_FLASH_BASE)
    add_compile_definitions(EXPECTED_CP_FLASH_BASE=${EXPECTED_CP_FLASH_BASE}U)
else()
    message(FATAL_ERROR "缺少 EXPECTED_CP_FLASH_BASE，应由 sample 顶层 CMake 提供")
endif()

add_subdirectory(src)

listenai_add_executable(${PROJECT_NAME})