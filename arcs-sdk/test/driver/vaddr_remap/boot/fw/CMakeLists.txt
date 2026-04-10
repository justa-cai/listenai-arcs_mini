cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

if(NOT DEFINED CONFIG_DEFAULT)
    set(CONFIG_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/prj.conf")
endif()

project(vaddr_remap_fw)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c)

listenai_set_linker_script(${ARCS_SDK_BASE}/soc/common/system.ld)
