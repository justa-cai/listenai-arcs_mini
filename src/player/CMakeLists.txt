listenai_library_named(player)

listenai_library_sources(
    audio_player.c
    listen_audiomgr.c
    listen_volume.c
    music_random.c
    music_manager.c
    play_mode.c
    short_player.c
    sound_player.c
    tts_player.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

listenai_link_libraries(utils)
