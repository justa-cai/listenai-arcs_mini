cmake_minimum_required(VERSION 3.10)

set(IPC_STUB_INCLUDES
    ${ARCS_HAL_LIB_PATH}/include/rtos
    ${ARCS_HAL_LIB_PATH}/include/debug
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/include
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/include/ipc
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/include/utils
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/driver/spiflash
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/rtos/rtos_al
)

set(IPC_COMMON_INCLUDES
    ${ARCS_HAL_LIB_PATH}/include/rtos
    ${ARCS_HAL_LIB_PATH}/include/debug
    ${ARCS_HAL_LIB_PATH}/include/driver
    ${ARCS_HAL_LIB_PATH}/include/NMSIS/Core/Include
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/include
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/include/wifi
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/include/bt
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/include/ipc
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/include/ipc/mrpc
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/include/utils
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/include/net
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/btos/task/api
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mrpc
    ${CMAKE_CURRENT_SOURCE_DIR}/service
    ${CMAKE_CURRENT_SOURCE_DIR}/service/wifi/mrpc
    ${CMAKE_CURRENT_SOURCE_DIR}/service/bt/mrpc
    ${CMAKE_CURRENT_SOURCE_DIR}/service/bt_s2m/mrpc
    ${CMAKE_CURRENT_SOURCE_DIR}/service/lwip/mrpc
    ${CMAKE_CURRENT_SOURCE_DIR}/service/nvs/mrpc
    ${CMAKE_CURRENT_SOURCE_DIR}/service/flash_if/mrpc
    ${CMAKE_CURRENT_SOURCE_DIR}/service/luna/mrpc
    ${CMAKE_CURRENT_SOURCE_DIR}/service/utils_s2m/mrpc
    ${CMAKE_CURRENT_SOURCE_DIR}/service/utils_m2s/mrpc
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/driver/private_include
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/driver/spiflash
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/rtos/rtos_al
    ${ARCS_HAL_LIB_PATH}/modules/rtos/portable/${BUILDTOOL}/non_secure
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/ic_lock
    ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/btos
)

if(DEFINED CONFIG_LWIP_VERSION)
    set(IPC_LWIP_VERSION_DIR "lwip-${CONFIG_LWIP_VERSION}")
    list(APPEND IPC_COMMON_INCLUDES
        ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/lwip/${IPC_LWIP_VERSION_DIR}/port/include
        ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/lwip/${IPC_LWIP_VERSION_DIR}/src/include
        ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/lwip/${IPC_LWIP_VERSION_DIR}/src/include/compat/posix
        ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/lwip/${IPC_LWIP_VERSION_DIR}/src/include/compat/stdc
        ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/lwip/${IPC_LWIP_VERSION_DIR}/src/include/lwip/apps
        ${ARCS_HAL_LIB_PATH}/chip/${CHIP}/lwip/${IPC_LWIP_VERSION_DIR}/contrib/ports/rtos/include
    )
endif()

listenai_include_directories(${ARCS_HAL_LIB_PATH}/chip/${CHIP}/include/ipc)

if(CONFIG_ARCS_HAL_IPC)
    listenai_library_named(ipc)
    target_include_directories(ipc PRIVATE ${IPC_COMMON_INCLUDES})
    target_compile_options(ipc PRIVATE -include arcs_ap.h)

    listenai_compile_definitions(CFG_AMP_IPC=1)

    if(CONFIG_WIFI_LWIP_DIFF_CORE)
        listenai_compile_definitions(
            CFG_AMP_IPC_WIFI_CHAN=1
            CFG_AMP_IPC_TCPIP=1
        )
    endif()

    # IPC log forwarding direction. Explicit writer/reader options take
    # precedence; when only the legacy CONFIG_ARCS_HAL_IPC_PRINT/CFG_IPC_PRINT
    # switch is set, fall back to the historical role-based direction
    # (slave = writer, master = reader).
    set(IPC_PRINT_WRITER FALSE)
    set(IPC_PRINT_READER FALSE)
    if(CONFIG_ARCS_HAL_IPC_PRINT_WRITER OR CONFIG_ARCS_HAL_IPC_PRINT_READER)
        set(IPC_PRINT_WRITER ${CONFIG_ARCS_HAL_IPC_PRINT_WRITER})
        set(IPC_PRINT_READER ${CONFIG_ARCS_HAL_IPC_PRINT_READER})
    elseif(CONFIG_ARCS_HAL_IPC_PRINT OR CFG_IPC_PRINT)
        if(CONFIG_ARCS_HAL_IPC_SLAVE)
            set(IPC_PRINT_WRITER TRUE)
        else()
            set(IPC_PRINT_READER TRUE)
        endif()
    endif()

    if(CONFIG_ARCS_HAL_IPC_PRINT OR CFG_IPC_PRINT)
        listenai_compile_definitions(CFG_IPC_PRINT=1)
    endif()
    if(IPC_PRINT_WRITER)
        listenai_compile_definitions(CFG_IPC_PRINT_WRITER=1)
    endif()
    if(IPC_PRINT_READER)
        listenai_compile_definitions(CFG_IPC_PRINT_READER=1)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MASTER)
        listenai_compile_definitions(CFG_AMP_IPC_MASTER=1)
    elseif(CONFIG_ARCS_HAL_IPC_SLAVE)
        listenai_compile_definitions(CFG_AMP_IPC_SLAVE=1)
    else()
        message(FATAL_ERROR "Invalid configuration: ARCS_HAL_IPC_MASTER or ARCS_HAL_IPC_SLAVE must be enabled")
    endif()

    if(CONFIG_ARCS_HAL_IPC_HALT_BY_PEER_CORE)
        listenai_compile_definitions(CFG_AMP_IPC_HALT_BY_PEER_CORE=1)
    endif()

    if(CONFIG_ARCS_HAL_IPC_HALT_PEER_CORE)
        listenai_compile_definitions(CFG_AMP_IPC_HALT_PEER_CORE=1)
    endif()

    if(CONFIG_ARCS_HAL_IPC_FLASH_AGENT)
        listenai_compile_definitions(CFG_AMP_IPC_FLASH_AGENT=1)
    endif()

    if(CFG_AMP_IPC_MASTER_INDICATION OR (CONFIG_WIFI_LWIP_DIFF_CORE AND CONFIG_ARCS_HAL_IPC_MASTER))
        listenai_compile_definitions(CFG_AMP_IPC_INDICATION=1)
    endif()

    if(CFG_AMP_IPC_BT_CHAN)
        listenai_compile_definitions(CFG_AMP_IPC_BT_CHAN=1)
    endif()

    if(CFG_IPC_TEST_CASE OR CONFIG_IPC_TEST_CASE)
        listenai_compile_definitions(IPC_TEST_CASE)
        target_include_directories(ipc PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/mrpc/test/master2slave
            ${CMAKE_CURRENT_SOURCE_DIR}/mrpc/test/slave2master
        )
    endif()

    listenai_library_sources(
        utils/ipc_utils.c
        utils/amp_shared.c
        core/ipc_core.c
        core/ipc_msg.c
        core/ipc_mem.c
        core/ipc_ep.c
        core/ipc_platform.c
        core/ipc_queue.c
        core/ipc_rx_task.c
        service/ipc_mrpc_service.c
    )

    if(CONFIG_ARCS_HAL_IPC_MASTER)
        listenai_library_sources(role/ipc_master.c)

        if(CONFIG_WIFI_LWIP_DIFF_CORE)
            listenai_library_sources(service/wifi/ipc_master_wifi.c)
        endif()

        if(CFG_AMP_IPC_BT_CHAN)
            listenai_library_sources(service/bt/ipc_master_bt.c)
        endif()

        if(CFG_IPC_TEST_CASE OR CONFIG_IPC_TEST_CASE)
            listenai_library_sources(
                mrpc/test/master2slave/mrpc_m2s_test_api_client.c
                mrpc/test/slave2master/mrpc_s2m_test_api_server.c
                core/ipc_test.c
            )
        endif()

    endif()

    if(CONFIG_ARCS_HAL_IPC_SLAVE)
        listenai_library_sources(
            role/ipc_slave.c
        )

        if(NOT CONFIG_ARCS_HAL_PM)
            listenai_library_sources(service/event/ipc_event_fwd.c)
        endif()

        if(CONFIG_WIFI_LWIP_DIFF_CORE)
            listenai_library_sources(service/wifi/ipc_slave_wifi.c)
        endif()

        if(CFG_AMP_IPC_BT_CHAN)
            listenai_library_sources(service/bt/ipc_slave_bt.c)
        endif()

        if(CFG_IPC_TEST_CASE OR CONFIG_IPC_TEST_CASE)
            listenai_library_sources(
                mrpc/test/master2slave/mrpc_m2s_test_api_server.c
                mrpc/test/slave2master/mrpc_s2m_test_api_client.c
                core/ipc_test.c
            )
        endif()

    endif()

    if(IPC_PRINT_READER)
        listenai_library_sources(debug/ipc_dbg_reader.c)
    endif()

    if(IPC_PRINT_WRITER)
        listenai_library_sources(debug/ipc_dbg_writer.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_SERVER=1)
        listenai_library_sources(mrpc/mrpc_server.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_CLIENT=1)
        listenai_library_sources(mrpc/mrpc_client.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_LWIP)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_SERVER_LWIP=1)
        listenai_library_sources(service/lwip/mrpc/mrpc_lwip_api_server.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_BT)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_SERVER_BT=1)
        listenai_library_sources(service/bt/mrpc/mrpc_bt_api_server.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_BT_S2M)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_SERVER_BT_S2M=1)
        listenai_library_sources(service/bt_s2m/mrpc/mrpc_bt_s2m_api_server.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_LWIP)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_CLIENT_LWIP=1)
        listenai_library_sources(service/lwip/mrpc/mrpc_lwip_api_client.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_BT)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_CLIENT_BT=1)
        listenai_library_sources(service/bt/mrpc/mrpc_bt_api_client.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_BT_S2M)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_CLIENT_BT_S2M=1)
        listenai_library_sources(service/bt_s2m/mrpc/mrpc_bt_s2m_api_client.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_WIFI)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_SERVER_WIFI=1)
        listenai_library_sources(service/wifi/mrpc/mrpc_wifi_api_server.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_WIFI)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_CLIENT_WIFI=1)
        listenai_library_sources(service/wifi/mrpc/mrpc_wifi_api_client.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_NVS)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_CLIENT_NVS=1)
        listenai_library_sources(service/nvs/mrpc/mrpc_nvs_api_client.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_NVS)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_SERVER_NVS=1)
        listenai_library_sources(service/nvs/mrpc/mrpc_nvs_api_server.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_FLASH_IF)
        listenai_compile_definitions(
            CFG_AMP_IPC_MRPC_CLIENT_FLASH_IF=1
            CFG_FLASH_IF=1
        )
        listenai_library_sources(service/flash_if/mrpc/mrpc_flash_if_api_client.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_FLASH_IF)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_SERVER_FLASH_IF=1)
        listenai_library_sources(service/flash_if/mrpc/mrpc_flash_if_api_server.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_UTILS_S2M)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_CLIENT_UTILS_S2M=1)
        listenai_library_sources(service/utils_s2m/mrpc/mrpc_utils_s2m_api_client.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_UTILS_S2M)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_SERVER_UTILS_S2M=1)
        listenai_library_sources(service/utils_s2m/mrpc/mrpc_utils_s2m_api_server.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_UTILS_M2S)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_CLIENT_UTILS_M2S=1)
        listenai_library_sources(service/utils_m2s/mrpc/mrpc_utils_m2s_api_client.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_UTILS_M2S)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_SERVER_UTILS_M2S=1)
        listenai_library_sources(service/utils_m2s/mrpc/mrpc_utils_m2s_api_server.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_LUNA)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_CLIENT_LUNA=1)
        listenai_library_sources(service/luna/mrpc/mrpc_luna_api_client.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_LUNA)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_SERVER_LUNA=1)
        listenai_library_sources(service/luna/mrpc/mrpc_luna_api_server.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_AP_PM_LOCK)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_CLIENT_AP_PM_LOCK=1)
        listenai_library_sources(service/ap_pm_lock/mrpc/mrpc_ap_pm_lock_api_client.c)
    endif()

    if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_AP_PM_LOCK)
        listenai_compile_definitions(CFG_AMP_IPC_MRPC_SERVER_AP_PM_LOCK=1)
        listenai_library_sources(service/ap_pm_lock/mrpc/mrpc_ap_pm_lock_api_server.c)
    endif()

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )
else()
    target_include_directories(${LISTENAI_CURRENT_LIBRARY} PRIVATE ${IPC_STUB_INCLUDES})
    listenai_library_sources(utils/ipc_utils_stub.c)
endif()
