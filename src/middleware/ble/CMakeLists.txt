listenai_library_named(app-ble)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

listenai_library_sources(
    app_ble_common.c
    app_net_cfg.c
)
