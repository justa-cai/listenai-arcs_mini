cmake_minimum_required(VERSION 3.20)

# Set global paths
set(TOPDIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)

# Include chip-specific configuration  
include(${TOPDIR}/chip/${CHIP}/arch/platform.cmake)

# Project setup
project(${TGT} C CXX ASM)

# Override linker script for this specific demo (can be set after include now)
#set(LDSCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/ram.ld)


# Default libraries - can be customized per demo
set(TGT_LIBS
    bsp
    drv
    unity
)

# Source files
file(GLOB TGT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.S
)

setup_project()

# Add subdirectories for common modules (libraries)
add_subdirectory(${TOPDIR}/chip/${CHIP}/bsp ${CMAKE_BINARY_DIR}/chip/${CHIP}/bsp)
add_subdirectory(${TOPDIR}/chip/${CHIP}/driver ${CMAKE_BINARY_DIR}/chip/${CHIP}/driver)
add_subdirectory(${TOPDIR}/chip/${CHIP}/modules/unity ${CMAKE_BINARY_DIR}/chip/${CHIP}/modules/unity)

message(STATUS "The following libs will be included: " "${TGT_LIBS}")

# Create the demo executable
create_target_executable(${TGT_NAME} "${TGT_SOURCES}" "${TGT_LIBS}")
