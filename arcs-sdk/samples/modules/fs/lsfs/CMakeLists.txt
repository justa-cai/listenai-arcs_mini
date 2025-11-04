cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_include_directories(
    ./
)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c src/list_dir_explicit.c)
if (CONFIG_DISK_DRIVER_SDMMC)
target_sources(${PROJECT_NAME} PRIVATE src/sdmmc_init.c)
endif()

if (CONFIG_DISK_DRIVER_FLASH)
target_compile_options(arcs-hal PRIVATE -DCONFIG_EXT_RAM)
endif()
