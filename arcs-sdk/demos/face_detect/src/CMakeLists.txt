listenai_library_named(app)

listenai_library_sources(
    main.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(button)
add_subdirectory(adc_key)
add_subdirectory(camera)
add_subdirectory(fd_image_stream)
add_subdirectory(display)
add_subdirectory(screen)
