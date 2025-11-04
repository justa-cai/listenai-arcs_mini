enable_language(ASM)

listenai_library_named(boot_app)

listenai_compile_definitions(-DCHIP=4 -DIC_BOARD=1 -DPSRAM_LOG_CHECK=0)

set(ENV{ARCS_SOC_BASE} $ENV{ARCS_BASE}/soc/arcs)

listenai_include_directories(
    $ENV{ARCS_SOC_BASE}/hal/chip/arcs/bsp
    $ENV{ARCS_SOC_BASE}/hal/chip/arcs/include
    $ENV{ARCS_SOC_BASE}/hal/include/bsp
    $ENV{ARCS_SOC_BASE}/hal/include/NMSIS/Core/Include

    $ENV{ARCS_SOC_BASE}/hal/include/bsp
    $ENV{ARCS_SOC_BASE}/hal/include/NMSIS/Core/Include
    $ENV{ARCS_SOC_BASE}/hal/include/NMSIS/DSP/Include
    $ENV{ARCS_SOC_BASE}/hal/include/NMSIS/DSP/Include/dsp
    $ENV{ARCS_SOC_BASE}/hal/include/NMSIS/DSP/PrivateInclude
    $ENV{ARCS_SOC_BASE}/hal/chip/arcs/bsp/
    $ENV{ARCS_SOC_BASE}/hal/chip/arcs/include/
    $ENV{ARCS_SOC_BASE}/hal/chip/arcs/include/utils
    $ENV{ARCS_SOC_BASE}/hal/chip/arcs/include/register
    $ENV{ARCS_SOC_BASE}/hal/chip/arcs/driver/private_include
    $ENV{ARCS_SOC_BASE}/hal/modules/debug/include/
    $ENV{ARCS_SOC_BASE}/hal/modules/debug/
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

listenai_library_sources(
    boot.S
    boot.c
    boot_log.c
    cache.c
    $ENV{ARCS_SOC_BASE}/hal/chip/arcs/driver/system/PSRAMManager.c
    $ENV{ARCS_SOC_BASE}/hal/chip/arcs/driver/cmu/clock_config.c
    $ENV{ARCS_SOC_BASE}/hal/chip/arcs/driver/cmu/ClockManager.c
    $ENV{ARCS_SOC_BASE}/hal/chip/arcs/driver/iomux/IOMuxManager.c
)
