cmake_minimum_required(VERSION 3.19)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})
project(arcs)

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/app_cae)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE src/main.c
    PRIVATE src/app_cae/app_cae.c
    PRIVATE src/app_cae/cae_audio_in.c
    PRIVATE src/app_cae/cae_audio_out.c
    PRIVATE src/app_cae/cae_uac.c
)
