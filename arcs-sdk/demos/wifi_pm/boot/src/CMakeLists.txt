set(ARCS_HAL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

enable_language(ASM)

listenai_library_named(boot_app)

listenai_library_sources(
    main.c
    boot.S
    ${ARCS_HAL_PATH}/soc/arcs/hal/chip/arcs/driver/cmu/clock_config.c
    ${ARCS_HAL_PATH}/soc/arcs/hal/chip/arcs/driver/cmu/ClockManager.c
)

listenai_include_directories(
    ${ARCS_HAL_PATH}/soc/arcs/hal/include/bsp
    ${ARCS_HAL_PATH}/soc/arcs/hal/include/NMSIS/Core/Include
    ${ARCS_HAL_PATH}/soc/arcs/hal/chip/arcs/include/
    ${ARCS_HAL_PATH}/soc/arcs/hal/chip/arcs/include/register
    ${ARCS_HAL_PATH}/soc/arcs/hal/chip/arcs/bsp/
    ${ARCS_HAL_PATH}/soc/arcs/hal/chip/arcs/driver/cmu/
    ${ARCS_HAL_PATH}/soc/arcs/hal/chip/arcs/driver/private_include/
)