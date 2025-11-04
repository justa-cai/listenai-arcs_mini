cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)
listenai_include_directories(
    ./
    fs/
)
listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE 
    test_stdio.c
    fs/sdmmc_init.c
    fs/user_fs.c
)

target_link_libraries(${PROJECT_NAME} PRIVATE unity)
