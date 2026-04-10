# cAT AT Command Parser - Unsolicited Example

cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(cat_unsolicited_example)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c)
