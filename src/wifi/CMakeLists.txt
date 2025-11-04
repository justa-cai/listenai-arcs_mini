listenai_library_named(wifi)

listenai_library_sources(
    listen_mac_manager.c
    listen_wifi.c
    low_wifi_cli.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
