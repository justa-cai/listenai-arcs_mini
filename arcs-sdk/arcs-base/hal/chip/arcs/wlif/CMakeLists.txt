cmake_minimum_required(VERSION 3.10)

if(CONFIG_ARCS_HAL_WLIF)
    # 静态库目标
    listenai_library_named(arcs_hal_wlif)

    if(CONFIG_ARCS_HAL_WCND_CFG_DPD_LUNA)
        target_compile_definitions(arcs_hal_wlif PRIVATE CFG_DPD_LUNA)
    endif()

    listenai_library_sources(
        wlif.c
    )

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )

    # 包含目录配置
    listenai_include_directories(
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
        ../../../modules/rtos/portable/${BUILDTOOL}/non_secure
        ../../../chip/${CHIP}/lwip/${LWIP_VER}/src/include
        ../../../chip/${CHIP}/lwip/${LWIP_VER}/src/include/compat/posix
        ../../../chip/${CHIP}/lwip/${LWIP_VER}/src/include/compat/stdc
        ../../../chip/${CHIP}/lwip/${LWIP_VER}/src/include/lwip/apps
        ../../../chip/${CHIP}/lwip/${LWIP_VER}/contrib/ports/rtos/include
    )
endif()
