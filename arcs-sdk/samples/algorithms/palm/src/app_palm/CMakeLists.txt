listenai_library_named(app_palm)

listenai_library_sources(
    app_palm.c
    app_palm_image_input.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
)
