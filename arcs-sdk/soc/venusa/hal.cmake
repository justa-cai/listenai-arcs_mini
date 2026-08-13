SET(VENUSA_HAL_LIB_PATH ${CMAKE_CURRENT_LIST_DIR}/hal)

enable_language(ASM)

listenai_library_named(hal)

# VENUSA XIP flash erase/program routines must run from SRAM.
#
# Venusa HAL demo builds default IC_BOARD=1. The SPI HAL uses IC_BOARD to choose
# real silicon clock ownership; when it is undefined, the preprocessor treats it
# as 0 and SPI_PowerControl() does not enable the CRM SPI clock.
listenai_compile_definitions(
    IC_BOARD=1
    CONFIG_EXT_RAM
)

# Driver sources
file(GLOB_RECURSE VENUSA_HAL_DRIVER_SRCS
    ${VENUSA_HAL_LIB_PATH}/driver/*.c
    ${VENUSA_HAL_LIB_PATH}/driver/*.S
)

# Exclude USB subsystems by default
list(FILTER VENUSA_HAL_DRIVER_SRCS EXCLUDE REGEX ".*/usb_subsys/.*")
list(FILTER VENUSA_HAL_DRIVER_SRCS EXCLUDE REGEX ".*/usb_subsys_cherry/.*")
list(FILTER VENUSA_HAL_DRIVER_SRCS EXCLUDE REGEX ".*/psram_unified/.*")

# BSP sources
file(GLOB_RECURSE VENUSA_HAL_BSP_SRCS
    ${VENUSA_HAL_LIB_PATH}/bsp/*.c
    ${VENUSA_HAL_LIB_PATH}/bsp/*.S
)

# Exclude BSP startup/intexc (replaced by SDK startup framework)
list(FILTER VENUSA_HAL_BSP_SRCS EXCLUDE REGEX ".*/gcc/startup_demosoc\\.S$")
list(FILTER VENUSA_HAL_BSP_SRCS EXCLUDE REGEX ".*/gcc/intexc_demosoc\\.S$")
# Exclude newlib stubs (provided by system/newlib_stubs.c)
list(FILTER VENUSA_HAL_BSP_SRCS EXCLUDE REGEX ".*/gcc/stubs/.*")
# Exclude system_RISCVN300.c (replaced by soc.c in SDK startup framework)
list(FILTER VENUSA_HAL_BSP_SRCS EXCLUDE REGEX ".*/system_RISCVN300\\.c$")
# Exclude debug utilities (replaced by SDK system/printk.c and system/log/)
list(FILTER VENUSA_HAL_DRIVER_SRCS EXCLUDE REGEX ".*/debug/tinyprintf\\.c$")
list(FILTER VENUSA_HAL_DRIVER_SRCS EXCLUDE REGEX ".*/debug/log_print\\.c$")

listenai_library_sources(
    ${VENUSA_HAL_DRIVER_SRCS}
    ${VENUSA_HAL_BSP_SRCS}
)

add_subdirectory(
    ${VENUSA_HAL_LIB_PATH}/driver/psram_unified
    ${CMAKE_BINARY_DIR}/soc/venusa/hal/driver/venusa_ps_ram_unified
)

# Collect all driver subdirectories for include paths
file(GLOB _VENUSA_HAL_DRIVER_ALL LIST_DIRECTORIES true
    ${VENUSA_HAL_LIB_PATH}/driver/*)
set(VENUSA_HAL_DRIVER_SUBDIRS "")
foreach(_entry ${_VENUSA_HAL_DRIVER_ALL})
    if(IS_DIRECTORY ${_entry})
        list(APPEND VENUSA_HAL_DRIVER_SUBDIRS ${_entry})
    endif()
endforeach()
list(FILTER VENUSA_HAL_DRIVER_SUBDIRS EXCLUDE REGEX ".*/usb_subsys")
list(FILTER VENUSA_HAL_DRIVER_SUBDIRS EXCLUDE REGEX ".*/usb_subsys_cherry")

# Include directories
listenai_include_directories(
    ${VENUSA_HAL_LIB_PATH}/include
    ${VENUSA_HAL_LIB_PATH}/include/NMSIS/Core/Include
    ${VENUSA_HAL_LIB_PATH}/include/NMSIS/DSP/Include
    ${VENUSA_HAL_LIB_PATH}/include/register
    ${VENUSA_HAL_LIB_PATH}/bsp
    ${VENUSA_HAL_LIB_PATH}/driver
    ${VENUSA_HAL_DRIVER_SUBDIRS}
)
