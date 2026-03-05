listenai_library_named(app_fd)

listenai_library_sources(
    app_fd.c
    app_fd_image_input.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
