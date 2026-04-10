if(CONFIG_ARCS_HAL_BT_HAL)
    listenai_library_named(arcs_hal_bt_hal)

    listenai_compile_definitions(
        CFG_CRYPTO_IC_MUTEX=1
    )

    listenai_library_sources(
        bt_app_hal.c
        bt_ble_hal.c
        bt_classic_hal.c
        bt_ip_hal.c
        bt_lea_hal.c
        bt_stack_hal.c
        bt_tmap_hal.c
    )

    if (CONFIG_BT_AUDIO)
        listenai_library_sources(
            bt_call_hal.c
            bt_music_hal.c
        )
    endif()

    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

endif()
    
