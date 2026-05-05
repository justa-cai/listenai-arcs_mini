cmake_minimum_required(VERSION 3.19)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(dual_core_ipc_ap)

listenai_add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE src/main.c)
