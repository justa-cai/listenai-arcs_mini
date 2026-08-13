# RTOS module - FreeRTOS
# FreeRTOS needs demo-specific FreeRTOSConfig.h

set(RTOS_MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(SUPPORTED_CHIPS "venusa" "vegah" "spica" "arcs")

if(DEFINED CHIP AND CHIP IN_LIST SUPPORTED_CHIPS)

    # FreeRTOS portable files for RISC-V Nuclei
    set(FREERTOS_PORTABLE_SOURCES
        ${RTOS_MODULE_DIR}/portable/riscv_nuclei/non_secure/port.c
        ${RTOS_MODULE_DIR}/portable/riscv_nuclei/non_secure/portasm.S
    )

else()
    message(FATAL_ERROR "FreeRTOS: CHIP not matched or defined.")
endif()

# Find FreeRTOS core source files
set(FREERTOS_CORE_SOURCES
    ${RTOS_MODULE_DIR}/croutine.c
    ${RTOS_MODULE_DIR}/event_groups.c
    ${RTOS_MODULE_DIR}/list.c
    ${RTOS_MODULE_DIR}/queue.c
    ${RTOS_MODULE_DIR}/stream_buffer.c
    ${RTOS_MODULE_DIR}/tasks.c
    ${RTOS_MODULE_DIR}/timers.c
)

# Memory management (heap_4 is commonly used)
set(FREERTOS_HEAP_SOURCE
    ${RTOS_MODULE_DIR}/portable/MemMang/heap_4.c
)

# Add all FreeRTOS sources to the library source
set(FREERTOS_SOURCES
    ${FREERTOS_CORE_SOURCES}
    ${FREERTOS_PORTABLE_SOURCES}
    ${FREERTOS_HEAP_SOURCE}
)

message(STATUS "RTOS sources: ${FREERTOS_SOURCES}")

create_module_library(rtos "${FREERTOS_SOURCES}")

target_include_directories(rtos PUBLIC
    ${RTOS_MODULE_DIR}/include
    ${RTOS_MODULE_DIR}/portable/riscv_nuclei/non_secure
)

# Provide path to FreeRTOSConfig.h
if(DEFINED CONFIG_FREERTOSCONFIG_H_PATH AND EXISTS ${CONFIG_FREERTOSCONFIG_H_PATH}/FreeRTOSConfig.h)
    target_include_directories(rtos PUBLIC ${CONFIG_FREERTOSCONFIG_H_PATH})
    message(STATUS "Using FreeRTOSConfig.h from ${CONFIG_FREERTOSCONFIG_H_PATH}")
else()
    target_include_directories(rtos PUBLIC ${TOPDIR}/chip/${CHIP}/rtos_default_config)
    message(STATUS "Using defualt FreeRTOSConfig.h")
endif()
