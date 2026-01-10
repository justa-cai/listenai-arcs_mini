listenai_library_named(ota_manager)

listenai_library_sources(
    ota_manager.c
    ota_api.c
    ota_flash.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
