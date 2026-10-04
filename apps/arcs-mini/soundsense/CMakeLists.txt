listenai_library_named(app-soundsense)

listenai_library_sources(
    ss_core.c
    ss_audio.c
    ss_shell.c
)

listenai_library_sources_ifdef(CONFIG_SOUNDSENSE_SCREEN ss_screen.c)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_TOP_DIR}/apps-ui
    ${PROJECT_TOP_DIR}/apps-ui/apps/llm
)
