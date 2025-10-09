
if(CONFIG_FLEXIBLE_BUTTON)

listenai_library_named(btn)

listenai_library_sources(
    lisa_btn.c
    FlexibleButton/flexible_button.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/FlexibleButton
)

endif()
