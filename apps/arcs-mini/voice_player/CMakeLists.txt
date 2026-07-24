listenai_library_named(app-voice-player)

listenai_library_sources(
    voice_player.c
    voice_player_alarm.c
    voice_player_music.c
    voice_player_tts.c
    tone_control/voice_player_network_tone.c
    tone_control/voice_player_tone.c
    tone_control/voice_player_wakeup_tone.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
