cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE 
    src/main.cpp
    src/io_stream_test.cpp
    src/fs.c
)

listenai_include_directories(src)

listenai_link_libraries(lsfs)
listenai_link_libraries(disk)
listenai_link_libraries(elmfat)
