cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_include_directories(
    ./
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/ipc
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/ipc/ipc_core
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/include/ipc
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/rtos/rtos_al
)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    test_ipc_mem_init_order.c
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/ipc/ipc_core/ipc_mem.c
)

target_link_libraries(${PROJECT_NAME} PRIVATE unity)
