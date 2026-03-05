cmake_minimum_required(VERSION 3.10)

if(CONFIG_ARCS_HAL_IPC)
    listenai_library_named(ipc)

    listenai_compile_definitions(
        CFG_AMP_IPC=1
    )

    # 双核WIFI模式才打开
    if(CONFIG_WIFI_LWIP_DIFF_CORE)
        listenai_compile_definitions(
            CFG_AMP_IPC_WIFI_CHAN=1
            CFG_AMP_IPC_TCPIP=1
        )
    endif()

    listenai_compile_definitions(
        # IPC_DEBUG=1
    )

    listenai_library_sources(
        ipc_core/ipc_core.c
        ipc_core/ipc_ep.c
        ipc_core/ipc_mem.c
        ipc_core/ipc_msg.c
        ipc_core/ipc_platform.c
        ipc_core/ipc_queue.c
        ipc_utils/ipc_utils.c
    )

    listenai_include_directories(
        ${ARCS_HAL_LIB_PATH}/chip/arcs/include/ipc/ipc_core
        ${ARCS_HAL_LIB_PATH}/chip/arcs/ipc/ipc_core
        ${ARCS_HAL_LIB_PATH}/chip/arcs/ipc/ipc_utils
        ${ARCS_HAL_LIB_PATH}/chip/arcs/driver/spiflash
    )

    # Master/Slave configuration
    if(CONFIG_ARCS_HAL_IPC_MASTER)
        listenai_compile_definitions(
            CFG_AMP_IPC_MASTER=1
        )
        listenai_library_sources(
            ipc_master/ipc_master.c
            ipc_master/ipc_print.c
        )
        if(CONFIG_WIFI OR CONFIG_LWIP)
            listenai_library_sources(
                ipc_master/ipc_master_wifi.c
            )
        endif()
    elseif(CONFIG_ARCS_HAL_IPC_SLAVE)
        listenai_compile_definitions(
            CFG_AMP_IPC_SLAVE=1
        )
        listenai_library_sources(
            ipc_slave/ipc_slave.c 
            ipc_slave/ipc_event_fwd.c
            ipc_slave/ipc_print.c
        )

        if(CONFIG_WIFI OR CONFIG_LWIP)
            listenai_library_sources(
                ipc_slave/ipc_slave_wifi.c
            )
        endif()

        listenai_include_directories(
            ipc_slave
        )
    else()
        message(FATAL_ERROR "Invalid configuration: ARCS_HAL_IPC_MASTER or ARCS_HAL_IPC_SLAVE must be enabled")
    endif()

    # MRPC Server
    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_SERVER=1
        )
        listenai_library_sources(
            mrpc/mrpc_server.c
        )
    endif()

    # MRPC Client
    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_CLIENT=1
        )
        listenai_library_sources(
            mrpc/mrpc_client.c
        )
    endif()

    # LWIP MRPC Server
    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_LWIP)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_SERVER_LWIP=1
        )
        listenai_library_sources(
            mrpc/lwip/mrpc_lwip_api_server.c
        )
    endif()

    # LWIP MRPC Client
    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_LWIP)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_CLIENT_LWIP=1
        )
        listenai_library_sources(
            mrpc/lwip/mrpc_lwip_api_client.c
        )
    endif()

    # WIFI MRPC Server
    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_WIFI)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_SERVER_WIFI=1
        )
        listenai_library_sources(
            mrpc/wifi/mrpc_wifi_api_server.c
        )
    endif()

    # WIFI MRPC Client
    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_WIFI)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_CLIENT_WIFI=1
        )
        listenai_library_sources(
            mrpc/wifi/mrpc_wifi_api_client.c
        )
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_NVS)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_SERVER_NVS=1
        )
        listenai_library_sources(
            mrpc/nvs/mrpc_nvs_api_server.c
        )
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_NVS)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_CLIENT_NVS=1
        )
        listenai_library_sources(
            mrpc/nvs/mrpc_nvs_api_client.c
        )
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_FLASH_IF)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_SERVER_FLASH_IF=1
        )
        listenai_library_sources(
            mrpc/flash_if/mrpc_flash_if_api_server.c
        )
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_FLASH_IF)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_CLIENT_FLASH_IF=1
            CFG_FLASH_IF=1
        )
        listenai_library_sources(
            mrpc/flash_if/mrpc_flash_if_api_client.c
        )
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_UTILS_S2M)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_CLIENT_UTILS_S2M=1
        )
        listenai_library_sources(
            mrpc/utils_s2m/mrpc_utils_s2m_api_client.c
        )
    endif()
    
    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_UTILS_S2M)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_SERVER_UTILS_S2M=1
        )
        listenai_library_sources(
            mrpc/utils_s2m/mrpc_utils_s2m_api_server.c
        )
    endif()


    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_UTILS_M2S)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_CLIENT_UTILS_M2S=1
        )
        listenai_library_sources(
            mrpc/utils_m2s/mrpc_utils_m2s_api_client.c
        )
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_UTILS_M2S)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_SERVER_UTILS_M2S=1
        )
        listenai_library_sources(
            mrpc/utils_m2s/mrpc_utils_m2s_api_server.c
        )
    endif()

    if(CONFIG_ARCS_HAL_IPC_HALT_BY_PEER_CORE)
        listenai_compile_definitions(
            CFG_AMP_IPC_HALT_BY_PEER_CORE=1
        )
    endif()

    if(CONFIG_ARCS_HAL_IPC_HALT_PEER_CORE)
        listenai_compile_definitions(
            CFG_AMP_IPC_HALT_PEER_CORE=1
        )
    endif()

    if(CONFIG_ARCS_HAL_IPC_FLASH_AGENT)
        listenai_compile_definitions(
            CFG_AMP_IPC_FLASH_AGENT=1
        )
    endif()

    # 包含目录配置
    listenai_include_directories(
        ../../../include/debug
        ../../../include/driver
        ../../../chip/${CHIP}/include
        ../../../chip/${CHIP}/include/wifi
        ../../../chip/${CHIP}/include/ipc
        ../../../chip/${CHIP}/include/utils
        ../../../chip/${CHIP}/include/net
        ../../../chip/${CHIP}/ipc/ipc_core
        ../../../chip/${CHIP}/ipc/mrpc/wifi
        ../../../chip/${CHIP}/ipc/mrpc/lwip
        ../../../chip/${CHIP}/driver/private_include
    )

    if(DEFINED CONFIG_LWIP_VERSION)
        # 可能LWIP没有打开，只能手动依赖
        set(LWIP_VERSION_DIR "lwip-${CONFIG_LWIP_VERSION}")
        listenai_include_directories(
            ../../../chip/${CHIP}/lwip/${LWIP_VERSION_DIR}/src/include
            ../../../chip/${CHIP}/lwip/${LWIP_VERSION_DIR}/src/include/compat/posix
            ../../../chip/${CHIP}/lwip/${LWIP_VERSION_DIR}/src/include/lwip/apps
            ../../../chip/${CHIP}/lwip/${LWIP_VERSION_DIR}/contrib/ports/rtos/include
        )
    endif()

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )
else()
    listenai_library_sources(ipc_utils/ipc_utils_stub.c)
endif()
