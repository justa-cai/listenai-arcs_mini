SET(ARCS_HAL_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/)

listenai_library_named(arcs-hal)

add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/driver)
add_subdirectory(${ARCS_HAL_LIB_PATH}/modules/vrtc)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/event)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/ipc)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/wlif)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/wcnd)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/cli_cmd)
add_subdirectory(${ARCS_HAL_LIB_PATH}/modules/shell)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/simsocket)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/lib)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/lwip)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/atcmd)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/flash_if)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/ic_lock)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/utils)
add_subdirectory(${ARCS_HAL_LIB_PATH}/modules/nvs)
add_subdirectory(${ARCS_HAL_LIB_PATH}/chip/arcs/btos)
add_subdirectory(${ARCS_HAL_LIB_PATH}/modules/profiles)

##部分依赖组件头文件！！！
listenai_include_directories(
    ${ARCS_HAL_LIB_PATH}/include/bsp
    ${ARCS_HAL_LIB_PATH}/include/NMSIS/Core/Include
    ${ARCS_HAL_LIB_PATH}/include/NMSIS/DSP/Include
    ${ARCS_HAL_LIB_PATH}/include/NMSIS/DSP/Include/dsp
    ${ARCS_HAL_LIB_PATH}/include/NMSIS/DSP/PrivateInclude
    ${ARCS_HAL_LIB_PATH}/chip/arcs/atcmd/
    ${ARCS_HAL_LIB_PATH}/chip/arcs/bsp/
    ${ARCS_HAL_LIB_PATH}/chip/arcs/include/
    ${ARCS_HAL_LIB_PATH}/chip/arcs/include/bt
    ${ARCS_HAL_LIB_PATH}/chip/arcs/include/utils
    ${ARCS_HAL_LIB_PATH}/chip/arcs/include/wifi
    ${ARCS_HAL_LIB_PATH}/chip/arcs/include/register
    ${ARCS_HAL_LIB_PATH}/chip/arcs/driver/private_include
    ${ARCS_HAL_LIB_PATH}/modules/debug/include/
    ${ARCS_HAL_LIB_PATH}/modules/debug/
    ${ARCS_HAL_LIB_PATH}/modules/ota/include
    ${ARCS_HAL_LIB_PATH}/modules/nvs/include
)

listenai_library_compile_options_ifdef(
    CONFIG_ARCS_HAL_WARNING_DISABLE
    PRIVATE
    -w
)

listenai_compile_definitions(
    CHIP=4 IC_BOARD=1
    IC_HAL_DCACHE_SIZE=32
    CONFIG_EXT_RAM
)

if (DEFINED CONFIG_ARCS_HAL_DCACHE_COHERENT)
    listenai_compile_definitions(IC_HAL_DCACHE_IS_COHERENT=1)
else()
    listenai_compile_definitions(IC_HAL_DCACHE_IS_COHERENT=0)
endif()

if (DEFINED CONFIG_BOOT_HART)
    listenai_compile_definitions(-DBOOT_HARTID=${CONFIG_HARTID})
else()
    listenai_compile_definitions(-DBOOT_HARTID=0)
endif()

if(${CONFIG_HARTID} EQUAL 0)
    listenai_compile_definitions(MBX_AP_WORK=1)
elseif(${CONFIG_HARTID} EQUAL 1)
    listenai_compile_definitions(MBX_CP_WORK=1)
endif()

listenai_compile_definitions_ifdef(CONFIG_MODULE_FREERTOS CFG_RTOS=1 IC_USE_RTOS=1)

