cmake_minimum_required(VERSION 3.13)

set(LISTENAI_MODULES_DIR_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(boot)

listenai_add_executable(${PROJECT_NAME})
listenai_set_linker_script(${CMAKE_CURRENT_SOURCE_DIR}/src/linker.ld)
