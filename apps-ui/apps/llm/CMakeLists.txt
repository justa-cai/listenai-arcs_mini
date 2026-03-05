target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lisa_ui_app.c
)

target_include_directories(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ./
)

# Only link lvgl library in simulator environment
if(CONFIG_LVGL_ENV_SIMULATOR)
    target_link_libraries(
        ${LISA_UI_LIB_NAME}
        PUBLIC
        lvgl
    )
endif()

add_subdirectory(models)
add_subdirectory(presenters)
add_subdirectory(views)
