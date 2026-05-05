cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(app_enter_boot_recovery_upgrade_fw)

if(NOT DEFINED OTA_CP_VERIFY_ADDR)
    message(FATAL_ERROR "OTA_CP_VERIFY_ADDR is required")
endif()
if(NOT DEFINED OTA_CP_VERIFY_MAGIC)
    message(FATAL_ERROR "OTA_CP_VERIFY_MAGIC is required")
endif()

listenai_add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE src/main.c)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    OTA_CP_VERIFY_ADDR=${OTA_CP_VERIFY_ADDR}
    OTA_CP_VERIFY_MAGIC="${OTA_CP_VERIFY_MAGIC}"
)
