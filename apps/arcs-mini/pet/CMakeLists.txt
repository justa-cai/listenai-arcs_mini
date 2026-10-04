listenai_library_named(app-pet)

listenai_library_sources(
    pet.c
    pet_core.c
    pet_draw.c
    pet_save.c
    pet_shell.c
    pet_ui.c
    pet_buzzer.c
    pet_voice.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_TOP_DIR}/apps-ui
    ${PROJECT_TOP_DIR}/apps-ui/apps/llm
)
