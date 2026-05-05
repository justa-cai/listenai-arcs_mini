cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(ipc_log_early_boot_ap)

listenai_add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE test_remote.c)
