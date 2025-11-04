if (DEFINED CONFIG_ARCS_HAL_CLI_CMD)
    listenai_library_named(arcs_hal_cli_cmd)

    # Add source files
    listenai_library_sources(
        cli_main.c
        cli_net.c
    )
    if (CONFIG_WIFI OR CONFIG_LWIP)
        listenai_library_sources(
            cli_wifi.c
        )
    endif()

    # Add include directories
    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/
    )

    if (DEFINED CONFIG_ARCS_HAL_CLI_CMD_CFG_WIFI_MFG)
        target_compile_definitions(arcs_hal_cli_cmd PRIVATE CFG_WIFI_MFG=1)
    endif()

    # Add compile definitions
    if (DEFINED WIFI_OTA)
        target_compile_definitions(arcs_hal_cli_cmd PRIVATE WIFI_OTA=${WIFI_OTA})
    endif()

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )

    # WCN type configuration
    if (DEFINED WCN_TYPE)
        message(STATUS "WCN_TYPE: ${WCN_TYPE}")
        if (${WCN_TYPE} STREQUAL "both" OR ${WCN_TYPE} STREQUAL "wf")
            target_compile_definitions(arcs_hal_cli_cmd PRIVATE CLI_TYPE_WF)
        endif()
    endif()

    if (CONFIG_WIFI OR CONFIG_LWIP)
        target_compile_definitions(arcs_hal_cli_cmd PRIVATE CLI_TYPE_WF)
    endif()

endif()
