listenai_library_named(app-button)

listenai_library_sources(
    app_button.c
    button_blecfg.c
    button_camera_preview.c
    button_factory_reset.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
