cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

# Main source files
target_sources(${PROJECT_NAME} PRIVATE 
    src/main.c
    src/camera/video_camera.c
    src/uvc/uvc_stream.c
    src/uvc/usb_descriptors.c
)

# Include directories
listenai_include_directories(
    ./
    src
    src/camera
    src/uvc
)

# Compiler optimizations
target_compile_options(${PROJECT_NAME} PRIVATE
    -Os
    -Wall
)
