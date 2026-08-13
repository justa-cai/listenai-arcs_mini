cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_include_directories(
    ./
)

listenai_compile_definitions(
    LISA_MODEM_EC801E_QIRD_PERF_LOG_ENABLE=1
)

add_subdirectory(src)

listenai_add_executable(${PROJECT_NAME})
listenai_code_relocate(LIBRARY chryusb LOCATION PSRAM)
