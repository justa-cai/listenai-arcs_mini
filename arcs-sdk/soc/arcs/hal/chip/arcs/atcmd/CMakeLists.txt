if (DEFINED CONFIG_ARCS_HAL_AT_CMD)
    listenai_library_named(arcs_hal_at_cmd)

    # Add source files
    listenai_library_sources(
        atcmd.c
        atcmd_hash.c
        atcmd_iperf.c
        atcmd_lwip.c
        atcmd_tcpip.c
        ping.c
    )

    if (CONFIG_WIFI)
        listenai_library_sources(
            atcmd_wifi.c
        )
    endif()

    if (CONFIG_BLE)
        listenai_library_sources(
            atcmd_bt.c
            atcmd_bt_if.c
        )
    endif()
    
    # Add include directories
    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/
    )

    listenai_compile_definitions(CFG_ATCMD=1)

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -Wno-error=incompatible-pointer-types
        -Wno-incompatible-pointer-types
    )

endif()
