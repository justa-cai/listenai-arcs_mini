cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})
listenai_include_directories(./)

target_sources(${PROJECT_NAME} PRIVATE src/main.c cc_custom.c)
