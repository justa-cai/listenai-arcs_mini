cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 使用本地 boot，跳过 SDK startup 中的 boot.cmake
set(SKIP_STARTUP_BOOT TRUE)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_include_directories(
    ./
)

listenai_compile_definitions(
    PACKET_TX_TIMEOUT_MS=300000U
    PACKET_RX_TIMEOUT_MS=300000U
)

add_subdirectory(src)

listenai_add_executable(${PROJECT_NAME})

listenai_set_linker_script(${CMAKE_CURRENT_SOURCE_DIR}/system.ld)

if (DEFINED CONFIG_BOOT)
    include(${CMAKE_CURRENT_SOURCE_DIR}/boot.cmake)
endif()
