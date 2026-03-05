listenai_library_named(app)

listenai_library_sources(
    main.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(app_fd)
