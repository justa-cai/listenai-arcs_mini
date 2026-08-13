cmake_minimum_required(VERSION 3.19)

if(NOT DEFINED ARCS_SDK_BASE)
    set(ARCS_SDK_BASE "$ENV{ARCS_BASE}")
endif()

if(NOT ARCS_SDK_BASE)
    get_filename_component(ARCS_SDK_BASE "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

if(NOT DEFINED ARCH)
    set(ARCH riscv)
endif()

include(${ARCS_SDK_BASE}/cmake/${ARCH}-toolchain.cmake)

set(LISTENAI_MODULES_DIR_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS ${ARCS_SDK_BASE})

project(boot)

listenai_add_executable(${PROJECT_NAME})
listenai_set_linker_script(${CMAKE_CURRENT_SOURCE_DIR}/src/linker.ld)
