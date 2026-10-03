if(CONFIG_GAMEPAD_ENABLE)

listenai_library_named(gamepad)

listenai_library_sources(
    gamepad.c
    gamepad_input.c
    gamepad_discovery.c
    gamepad_rom.c
    gamepad_udp.c
    gamepad_ws_server.c
    gamepad_util.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

endif()
