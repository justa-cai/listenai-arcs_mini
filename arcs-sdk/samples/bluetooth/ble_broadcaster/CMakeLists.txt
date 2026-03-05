cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_include_directories(
    ./
)

add_subdirectory(src)


listenai_add_executable(${PROJECT_NAME})