cmake_minimum_required(VERSION 3.13)

# 预设置LISTENAI_MODULES_DIR_LIST以避免警告
if(NOT DEFINED LISTENAI_MODULES_DIR_LIST AND DEFINED ENV{ARCS_BASE})
    set(LISTENAI_MODULES_DIR_LIST "$ENV{ARCS_BASE}")
endif()

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(app)

listenai_add_executable(${PROJECT_NAME})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_sources(${PROJECT_NAME} PRIVATE src/main.c)
