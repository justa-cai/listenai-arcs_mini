cmake_minimum_required(VERSION 3.13)

if(CONFIG_WIFI)
    if(DEFINED CONFIG_FPU)
        set(FLOAT_DIR ilp32f)
    else()
        set(FLOAT_DIR ilp32)
    endif()

    if(CONFIG_WIFI_LWIP_DIFF_CORE)
        listenai_compile_definitions(CFG_AMP_IPC=1)
    endif()
    

    listenai_link_libraries(
        ${CMAKE_CURRENT_SOURCE_DIR}/${FLOAT_DIR}/libplf.a
        ${CMAKE_CURRENT_SOURCE_DIR}/${FLOAT_DIR}/libmodule.a
        ${CMAKE_CURRENT_SOURCE_DIR}/${FLOAT_DIR}/libmoduledp.a
        ${CMAKE_CURRENT_SOURCE_DIR}/${FLOAT_DIR}/libumac.a
        ${CMAKE_CURRENT_SOURCE_DIR}/${FLOAT_DIR}/libumacdp.a
        ${CMAKE_CURRENT_SOURCE_DIR}/${FLOAT_DIR}/liblmac.a
        ${CMAKE_CURRENT_SOURCE_DIR}/${FLOAT_DIR}/liblmacdp.a
        ${CMAKE_CURRENT_SOURCE_DIR}/${FLOAT_DIR}/libwpa_supplicant_ap.a
        ${CMAKE_CURRENT_SOURCE_DIR}/${FLOAT_DIR}/libwpa_supplicant_sta.a
    )
endif() # CONFIG_WIFI

if(CONFIG_WIFI_TX_BUF_COPY)
    listenai_compile_definitions(TX_BUF_COPY=1)
endif()

if(CONFIG_BLE)
    set(FLOAT_DIR ble32)

    listenai_link_libraries(
        ${CMAKE_CURRENT_SOURCE_DIR}/${FLOAT_DIR}/libble.a
        ${CMAKE_CURRENT_SOURCE_DIR}/${FLOAT_DIR}/libbt_base.a
        ${CMAKE_CURRENT_SOURCE_DIR}/${FLOAT_DIR}/libstack.a
    )

endif() # CONFIG_BLE
