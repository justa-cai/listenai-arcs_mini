cmake_minimum_required(VERSION 3.13)

set(LISTENAI_MODULES_DIR_LIST
    $ENV{ARCS_BASE}
    $ENV{ARCS_BASE}/../modules/libpng
    $ENV{ARCS_BASE}/../modules/zlib
    $ENV{ARCS_BASE}/../modules/easylogger
    $ENV{ARCS_BASE}/../components
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})


include_directories(
    ${CMAKE_SOURCE_DIR}/modules/libpng/libpng16
    ${CMAKE_SOURCE_DIR}/modules/zlib/include
)

project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c)


