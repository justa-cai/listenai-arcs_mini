cmake_minimum_required(VERSION 3.13)

set(LISTENAI_MODULES_DIR_LIST
    $ENV{ARCS_BASE}
    $ENV{ARCS_BASE}/../components/ipclsf
    $ENV{ARCS_BASE}/../components/lisa_log
    $ENV{ARCS_BASE}/../modules/easylogger
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c)
