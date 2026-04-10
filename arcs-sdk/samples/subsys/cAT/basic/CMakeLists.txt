# cAT AT Command Parser - Basic Example

cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c)
