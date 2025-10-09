if (CONFIG_ARCS_HAL_SIMSOCKET)
    file(GLOB_RECURSE SOURCES
        "*.c"
    )
    set(TARGET_NAME arcs_hal_simsocket)
    listenai_library_named(${TARGET_NAME})

    listenai_compile_definitions(LWIP_SOCKET=1 LWIP_POSIX_SOCKETS_IO_NAMES=1 LWIP_COMPAT_SOCKETS=1)

    listenai_library_sources(${SOURCES})

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )

    # 包含目录配置
    listenai_include_directories(
        ${ARCS_HAL_LIB_PATH}/include/NMSIS/Core/Include
        ../../../include/NMSIS/Core/Include
        ../../../include/bsp
        ../../../chip/${CHIP}/simsocket
        ../../../chip/${CHIP}/include
        ../../../chip/${CHIP}/include/wifi
        ../../../chip/${CHIP}/include/utils
        ../../../chip/${CHIP}/ipc/ipc_core
        ../../../chip/${CHIP}/ipc/ipc_utils
        ../../../chip/${CHIP}/include/ipc
        ../../../chip/${CHIP}/include/net
        ../../../chip/${CHIP}/lwip/${LWIP_VER}/src/include
        ../../../chip/${CHIP}/lwip/${LWIP_VER}/src/include/compat/posix
        ../../../chip/${CHIP}/lwip/${LWIP_VER}/src/include/compat/stdc
        ../../../chip/${CHIP}/lwip/${LWIP_VER}/src/include/lwip/apps
        ../../../chip/${CHIP}/lwip/port/include
    )

    # 编译定义
    target_compile_definitions(${TARGET_NAME} PRIVATE CFG_SIM_SOCKET=1)
endif()
