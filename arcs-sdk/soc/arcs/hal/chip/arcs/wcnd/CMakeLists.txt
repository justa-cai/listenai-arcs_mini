cmake_minimum_required(VERSION 3.10)

if(CONFIG_ARCS_HAL_WCND)
    # 静态库目标
    listenai_library_named(arcs_hal_wcnd)
    listenai_add_linker_section(FILE ${CMAKE_CURRENT_SOURCE_DIR}/wcnd-rodata.ld SLOT ROM SORT_KEY "20-hal")

    if (CONFIG_WIFI AND CONFIG_BT_DUAL)
        listenai_compile_definitions(WCN_TYPE=both)
        listenai_compile_definitions(RFCALI_BT_EN=1 WCN_TYPE_BT=1)
        listenai_compile_definitions(RFCALI_WF_EN=1 WCN_TYPE_WF=1)
        listenai_compile_definitions(WCN_TYPE_BT_DUAL=1)
        listenai_compile_definitions(BT_WIFI_COEX=1) # 启用蓝牙相关的接口和AT指令功能
    elseif(CONFIG_WIFI AND CONFIG_BLE)
        listenai_compile_definitions(WCN_TYPE=both)
        listenai_compile_definitions(RFCALI_BT_EN=1 WCN_TYPE_BT=1)
        listenai_compile_definitions(RFCALI_WF_EN=1 WCN_TYPE_WF=1)
        listenai_compile_definitions(BT_WIFI_COEX=1) # 启用蓝牙相关的接口和AT指令功能
    elseif (CONFIG_WIFI)
        listenai_compile_definitions(WCN_TYPE=wf)
        listenai_compile_definitions(RFCALI_WF_EN=1 WCN_TYPE_WF=1)
    elseif (CONFIG_BLE)
        listenai_compile_definitions(WCN_TYPE=bt)
        listenai_compile_definitions(RFCALI_BT_EN=1 WCN_TYPE_BT=1)
        listenai_compile_definitions(RFCALI_WF_EN=1)
        listenai_compile_definitions(BT_WIFI_COEX=1) # 启用蓝牙相关的接口和AT指令功能
    endif()

    if (CONFIG_WIFI_LWIP_SAME_CORE)
        listenai_library_sources(${CMAKE_CURRENT_SOURCE_DIR}/misc/ls_misc.c)
    endif()

    if (CONFIG_BLE OR CONFIG_BT_DUAL)
        listenai_library_sources(
            ${CMAKE_CURRENT_SOURCE_DIR}/bt_drv/bt_drv_alarm.c
            ${CMAKE_CURRENT_SOURCE_DIR}/bt_drv/bt_drv_debug.c
            ${CMAKE_CURRENT_SOURCE_DIR}/bt_drv/bt_drv_ext_cfg.c
            ${CMAKE_CURRENT_SOURCE_DIR}/bt_drv/bt_drv_if.c
            ${CMAKE_CURRENT_SOURCE_DIR}/bt_drv/bt_drv_rf_api.c
            ${CMAKE_CURRENT_SOURCE_DIR}/bt_drv/bt_drv.c
            ${CMAKE_CURRENT_SOURCE_DIR}/bt_drv/bt_glb_int.c
            ${CMAKE_CURRENT_SOURCE_DIR}/bt_drv/bt_nvs_configure.c
        )

        listenai_library_sources(
            ${CMAKE_CURRENT_SOURCE_DIR}/dma/ls_dma.c
            ${CMAKE_CURRENT_SOURCE_DIR}/sleep/ls_sleep.c
            ${CMAKE_CURRENT_SOURCE_DIR}/timer/timer.c
        )

        listenai_include_directories(
            ${ARCS_HAL_LIB_PATH}/chip/arcs/wcnd/include/bt_inc
        )


        if(CONFIG_ARCS_HAL_IPC_MRPC_SERVER_BT)
            listenai_library_sources(
                ${CMAKE_CURRENT_SOURCE_DIR}/bt_drv/bt_ipc_api.c
            )
        endif()

    endif() # CONFIG_BLE

    file(GLOB WCND_SRCS
        ${ARCS_HAL_LIB_PATH}/chip/arcs/wcnd/misc/*.c
        ${ARCS_HAL_LIB_PATH}/chip/arcs/wcnd/nv/*.c
        ${ARCS_HAL_LIB_PATH}/chip/arcs/wcnd/rf/*.c
        ${ARCS_HAL_LIB_PATH}/chip/arcs/wcnd/wf_drv/*.c
    )
    if (CONFIG_ARCS_HAL_WCND_RF_SELFCALI_FROM_NV)
        listenai_compile_definitions(RF_SELF_CALI_FROM_NV=1)
    endif()
    if (CONFIG_ARCS_HAL_WCND_RF_SELFCALI_WRITE_TO_NV)
        listenai_compile_definitions(RF_SELF_CALI_WRITE_TO_NV=1)
    endif()
    if (CONFIG_ARCS_HAL_WCND_USE_FLASH_OTP)
        listenai_compile_definitions(USE_FLASH_OTP=1)
    endif()

    if(CONFIG_WIFI_CALI_BUF)
        listenai_compile_definitions(CALI_BUF=1)
    endif()

    if(CONFIG_ARCS_HAL_WCND_CFG_DPD_LUNA)
        target_compile_definitions(arcs_hal_wcnd PRIVATE CFG_DPD_LUNA)
    else()
        list(FILTER WCND_SRCS EXCLUDE REGEX "DpdEstLuna.c")
    endif()
    
    target_compile_definitions(arcs_hal_wcnd PRIVATE RF_BOARD_VER=${CONFIG_ARCS_HAL_WCND_RF_BOARD_VER})

    listenai_compile_definitions(
        FLASH_WF_MFG_CONF_BASE_ADDR=${CONFIG_ARCS_HAL_WCND_FLASH_WF_MFG_CONF_BASE_ADDR}
        WIFI_RF_SET_GOLDEN=${CONFIG_ARCS_HAL_WCND_WIFI_RF_SET_GOLDEN}
        XO_CAP_GOLDEN_VAL=${CONFIG_ARCS_HAL_WCND_XO_CAP_GOLDEN_VAL}
        PWR_OFFSET_LOW_GOLDEN_VAL=${CONFIG_ARCS_HAL_WCND_PWR_OFFSET_LOW_GOLDEN_VAL}
        PWR_OFFSET_MID_GOLDEN_VAL=${CONFIG_ARCS_HAL_WCND_PWR_OFFSET_MID_GOLDEN_VAL}
        PWR_OFFSET_HIGH_GOLDEN_VAL=${CONFIG_ARCS_HAL_WCND_PWR_OFFSET_HIGH_GOLDEN_VAL}
        RSSI_OFFSET_DSSS_GOLDEN_VAL=${CONFIG_ARCS_HAL_WCND_RSSI_OFFSET_DSSS_GOLDEN_VAL}
        RSSI_OFFSET_OFDM_GOLDEN_VAL=${CONFIG_ARCS_HAL_WCND_RSSI_OFFSET_OFDM_GOLDEN_VAL}
    )

    listenai_library_sources(${WCND_SRCS})


    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )

    listenai_include_directories(
        ${ARCS_HAL_LIB_PATH}/chip/arcs/include/net
        ${ARCS_HAL_LIB_PATH}/chip/arcs/wcnd/include
        ${ARCS_HAL_LIB_PATH}/chip/arcs/include
    )
endif()
