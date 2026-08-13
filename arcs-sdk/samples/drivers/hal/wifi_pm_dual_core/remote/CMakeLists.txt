cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(ap)

listenai_include_directories(./)

if(NOT DEFINED EXPECTED_CP_FLASH_BASE)
    set(EXPECTED_CP_FLASH_BASE 0x30100000)
endif()
if(NOT DEFINED EXPECTED_AP_PSRAM_BASE)
    set(EXPECTED_AP_PSRAM_BASE 0x28000000)
endif()
if(NOT DEFINED EXPECTED_AP_PSRAM_SIZE)
    set(EXPECTED_AP_PSRAM_SIZE 0x00400000)
endif()

listenai_compile_definitions(
    EXPECTED_CP_FLASH_BASE=${EXPECTED_CP_FLASH_BASE}
    EXPECTED_AP_PSRAM_BASE=${EXPECTED_AP_PSRAM_BASE}
    EXPECTED_AP_PSRAM_SIZE=${EXPECTED_AP_PSRAM_SIZE}
)

add_subdirectory(src)

listenai_compile_definitions(
    PM_CORE_PRIMARY=0
    CFG_VRTC_PROXY=1
)

listenai_add_executable(${PROJECT_NAME})
