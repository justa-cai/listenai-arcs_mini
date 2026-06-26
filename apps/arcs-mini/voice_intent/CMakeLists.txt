listenai_library_named(app-voice-intent)
listenai_library_sources(
    voice_intent_mgr.c
    voice_intent_music.c
    voice_intent_prompt_tone.c
    voice_intent_session.c
    voice_intent_photo_flow.c
    voice_intent_alarm.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
