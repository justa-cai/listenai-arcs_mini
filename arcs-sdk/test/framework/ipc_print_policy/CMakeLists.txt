cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_include_directories(
    ./
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/ipc/include
)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    test_ipc_print_policy.c
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CONFIG_ARCS_HAL_IPC_PRINT_NOTIFY_THRESHOLD=64
    CONFIG_ARCS_HAL_IPC_PRINT_POLL_TIMEOUT_MS=1234
)

target_link_libraries(${PROJECT_NAME} PRIVATE unity)
