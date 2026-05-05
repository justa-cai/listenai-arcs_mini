
listenai_library_sources(
    powerup.c
    platform.c
    sys_network_manager.c
    remote_logger.c
    sys_network_probe.c
    sys_wifi.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
