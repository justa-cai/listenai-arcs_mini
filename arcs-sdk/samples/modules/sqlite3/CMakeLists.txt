cmake_minimum_required(VERSION 3.13)

set(LISTENAI_MODULES_DIR_LIST 
    $ENV{ARCS_BASE}
    $ENV{ARCS_BASE}/../modules
    $ENV{ARCS_BASE}/../components
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(scanpen)

listenai_add_executable(${PROJECT_NAME})

if (CONFIG_SDK_MODULE_MBEDTLS)
listenai_include_directories(${PROJECT_SOURCE_DIR}/src)
endif()


# 添加项目代码到构建系统中
target_sources(${PROJECT_NAME} PRIVATE 
    src/main.c
    src/sdmmc_init.c)

target_compile_options(arcs-hal PRIVATE -DCONFIG_EXT_RAM)
