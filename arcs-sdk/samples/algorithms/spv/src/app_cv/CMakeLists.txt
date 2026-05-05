listenai_library_named(app_cv)

listenai_library_sources(
    app_cv.c
    app_cv_image_input.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
)
