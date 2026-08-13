cmake_minimum_required(VERSION 3.20)

# Set global paths
set(TOPDIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)

# Include chip-specific configuration  
include(${TOPDIR}/chip/${CHIP}/arch/platform.cmake)

# Project setup
project(${TGT} C CXX ASM)

# Override linker script for this specific demo
set(LDSCRIPT ${CMAKE_SOURCE_DIR}/ram.ld)


# Export and define global flags
set(SMP_CPU_CNT 2)
set(CONFIG_USE_SMP 1)
set(CFG_RTOS 1)
add_compile_definitions(SMP_CPU_CNT=2)
add_compile_definitions(CONFIG_USE_SMP)
add_compile_definitions(CFG_RTOS)


# Provide path to FreeRTOSConfig.h as a compile-time macro if present
set(CONFIG_FREERTOSCONFIG_H_PATH ${CMAKE_CURRENT_SOURCE_DIR})
if(EXISTS ${CONFIG_FREERTOSCONFIG_H_PATH}/FreeRTOSConfig.h)
    add_compile_definitions(CONFIG_FREERTOSCONFIG_H_PATH="${CONFIG_FREERTOSCONFIG_H_PATH}")
endif()

# Default libraries - can be customized per demo
set(TGT_LIBS
    bsp
    drv
    rtos
)



# Source files
file(GLOB TGT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.S
)


include_directories(
    ${TOPDIR}/chip/${CHIP}/modules/rtos/include
    ${TOPDIR}/chip/${CHIP}/modules/rtos/portable/riscv_nuclei/non_secure
)

setup_project()

# Add subdirectories for common modules (libraries)
add_subdirectory(${TOPDIR}/chip/${CHIP}/bsp ${CMAKE_BINARY_DIR}/chip/${CHIP}/bsp)
add_subdirectory(${TOPDIR}/chip/${CHIP}/driver ${CMAKE_BINARY_DIR}/chip/${CHIP}/driver)
add_subdirectory(${TOPDIR}/chip/${CHIP}/modules/rtos ${CMAKE_BINARY_DIR}/chip/${CHIP}/modules/rtos)



message(STATUS "The following libs will be included: " "${TGT_LIBS}")

# Create the demo executable
create_target_executable(${TGT_NAME} "${TGT_SOURCES}" "${TGT_LIBS}")


