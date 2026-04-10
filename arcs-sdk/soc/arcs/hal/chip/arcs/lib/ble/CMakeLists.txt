if(CONFIG_BLE AND NOT CONFIG_BT_DUAL)
    if(DEFINED CONFIG_FPU)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ilpf32)
            set(BT_FLOAT_DIR ilpf32)
        else()
            set(BT_FLOAT_DIR ilp32f)
        endif()
    else()
        set(BT_FLOAT_DIR ilp32)
    endif()

    if(CONFIG_LISA_BLUETOOTH_CLASSIC)
        set(BT_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bt_dual/${BT_FLOAT_DIR})
        listenai_link_libraries(
            ${BT_LIB_DIR}/libble.a
            ${BT_LIB_DIR}/libbt_base.a
            ${BT_LIB_DIR}/libstack.a
            ${BT_LIB_DIR}/libbt.a
            ${BT_LIB_DIR}/libbt_music.a
            ${BT_LIB_DIR}/libbt_call.a
        )
    else()
        set(BT_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${BT_FLOAT_DIR})
        listenai_link_libraries(
            ${BT_LIB_DIR}/libble.a
            ${BT_LIB_DIR}/libbt_base.a
            ${BT_LIB_DIR}/libstack.a
        )
    endif()
endif() # CONFIG_BLE
