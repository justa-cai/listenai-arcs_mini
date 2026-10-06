if(CONFIG_LISA_HTTP)

listenai_library_named(romlib)

listenai_library_sources(
    rom_api.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

endif()
