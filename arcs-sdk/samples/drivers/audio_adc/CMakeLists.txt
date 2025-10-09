cmake_minimum_required(VERSION 3.13)

set(LISTENAI_MODULES_DIR_LIST
    $ENV{ARCS_BASE}
    $ENV{ARCS_BASE}/../components
    $ENV{ARCS_BASE}/../modules
)

# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_include_directories(
    ./
)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c)
