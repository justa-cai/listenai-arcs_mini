cmake_minimum_required(VERSION 3.20)

# Set global paths
set(TOPDIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)

# Include chip-specific configuration  
include(${TOPDIR}/chip/${CHIP}/arch/platform.cmake)

# Project setup
project(${TGT} C CXX ASM)

# Override linker script for this specific demo (can be set after include now)
set(LDSCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/ram.ld)


# Default libraries - can be customized per demo
set(TGT_LIBS
    bsp
    drv
    rtos
    unity
)

# Source files
file(GLOB TGT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.S
)

# Export and define global flags
set(CFG_RTOS 1)
add_compile_definitions(CFG_RTOS)

 
# Provide path to FreeRTOSConfig.h as a compile-time macro if present
set(CONFIG_FREERTOSCONFIG_H_PATH ${CMAKE_CURRENT_SOURCE_DIR})
if(EXISTS ${CONFIG_FREERTOSCONFIG_H_PATH}/FreeRTOSConfig.h)
    add_compile_definitions(CONFIG_FREERTOSCONFIG_H_PATH="${CONFIG_FREERTOSCONFIG_H_PATH}")
endif()

set(CONFIG_USB_SUBSYS CHERRY)

include_directories(
    ${TOPDIR}/chip/${CHIP}/modules/rtos/include
    ${TOPDIR}/chip/${CHIP}/modules/rtos/portable/riscv_nuclei/non_secure
)

setup_project()

# Add subdirectories for common modules (libraries)
add_subdirectory(${TOPDIR}/chip/${CHIP}/bsp ${CMAKE_BINARY_DIR}/chip/${CHIP}/bsp)
add_subdirectory(${TOPDIR}/chip/${CHIP}/driver ${CMAKE_BINARY_DIR}/chip/${CHIP}/driver)
add_subdirectory(${TOPDIR}/chip/${CHIP}/modules/rtos ${CMAKE_BINARY_DIR}/chip/${CHIP}/modules/rtos)
add_subdirectory(${TOPDIR}/chip/${CHIP}/modules/unity ${CMAKE_BINARY_DIR}/chip/${CHIP}/modules/unity)

message(STATUS "The following libs will be included: " "${TGT_LIBS}")

# Create the demo executable
create_target_executable(${TGT_NAME} "${TGT_SOURCES}" "${TGT_LIBS}")
