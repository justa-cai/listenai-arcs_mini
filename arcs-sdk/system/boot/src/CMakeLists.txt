enable_language(ASM)

listenai_library_named(boot_app)

listenai_compile_definitions(-DCHIP=4 -DIC_BOARD=1 -DPSRAM_LOG_CHECK=0)

if(DEFINED BOOT_EXTRA_DEFINES)
    string(REPLACE "|" ";" BOOT_EXTRA_DEFINES_LIST "${BOOT_EXTRA_DEFINES}")
    foreach(define IN LISTS BOOT_EXTRA_DEFINES_LIST)
        listenai_compile_definitions(${define})
    endforeach()
endif()

set(ARCS_SOC_BASE ${ARCS_SDK_BASE}/soc/arcs)

listenai_include_directories(
    ${ARCS_SOC_BASE}/hal/chip/arcs/bsp
    ${ARCS_SOC_BASE}/hal/chip/arcs/include
    ${ARCS_SOC_BASE}/hal/include/bsp
    ${ARCS_SOC_BASE}/hal/include/NMSIS/Core/Include

    ${ARCS_SOC_BASE}/hal/include/bsp
    ${ARCS_SOC_BASE}/hal/include/NMSIS/Core/Include
    ${ARCS_SOC_BASE}/hal/include/NMSIS/DSP/Include
    ${ARCS_SOC_BASE}/hal/include/NMSIS/DSP/Include/dsp
    ${ARCS_SOC_BASE}/hal/include/NMSIS/DSP/PrivateInclude
    ${ARCS_SOC_BASE}/hal/chip/arcs/bsp/
    ${ARCS_SOC_BASE}/hal/chip/arcs/include/
    ${ARCS_SOC_BASE}/hal/chip/arcs/include/utils
    ${ARCS_SOC_BASE}/hal/chip/arcs/include/register
    ${ARCS_SOC_BASE}/hal/chip/arcs/driver/private_include
    ${ARCS_SOC_BASE}/hal/modules/debug/include/
    ${ARCS_SOC_BASE}/hal/modules/debug/
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

listenai_library_sources(
    boot.S
    boot.c
    boot_log.c
    cache.c
    ${ARCS_SOC_BASE}/hal/chip/arcs/driver/system/PSRAMManager.c
    ${ARCS_SOC_BASE}/hal/chip/arcs/driver/cmu/clock_config.c
    ${ARCS_SOC_BASE}/hal/chip/arcs/driver/cmu/ClockManager.c
    ${ARCS_SOC_BASE}/hal/chip/arcs/driver/iomux/IOMuxManager.c
)
