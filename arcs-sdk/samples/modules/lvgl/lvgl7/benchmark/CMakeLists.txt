cmake_minimum_required(VERSION 3.13)

set(LISTENAI_MODULES_DIR_LIST
    $ENV{ARCS_BASE}
    $ENV{ARCS_BASE}/../modules/lvgl
    $ENV{ARCS_BASE}/../modules/easylogger
    $ENV{ARCS_BASE}/../components
)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(app)

listenai_add_executable(${PROJECT_NAME})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_sources(${PROJECT_NAME} PRIVATE src/main.c)
