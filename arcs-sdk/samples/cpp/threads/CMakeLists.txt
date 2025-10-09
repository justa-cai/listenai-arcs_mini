cmake_minimum_required(VERSION 3.13)

set(LISTENAI_MODULES_DIR_LIST
    $ENV{ARCS_BASE}
    $ENV{ARCS_BASE}/../modules/FreeRTOS-CPP11
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE 
    src/main.cpp
    src/future_promise_test.cpp
    src/mutex_condvar_test.cpp
)

listenai_include_directories(src)
