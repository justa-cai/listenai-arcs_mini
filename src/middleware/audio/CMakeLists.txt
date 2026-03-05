listenai_library_named(audio)

listenai_library_sources(
    app_player.c
    audio_play.c
    pa_manager.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
