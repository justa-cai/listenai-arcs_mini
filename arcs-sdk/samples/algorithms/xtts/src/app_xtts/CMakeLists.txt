listenai_library_named(app_xtts)

listenai_library_sources(
    app_xtts.c
    xtts_audio_out.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
