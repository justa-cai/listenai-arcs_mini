cmake_minimum_required(VERSION 3.13)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(app)

listenai_add_executable(${PROJECT_NAME})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_sources(${PROJECT_NAME} PRIVATE src/main.c)
