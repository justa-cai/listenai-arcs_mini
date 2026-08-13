listenai_library_named(app)

listenai_library_sources(
    main.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FACE_DETECT_VERSION_HEADER_DIR}
)

if(CONFIG_BUTTON)
    add_subdirectory(button)
endif()

add_subdirectory(camera)
add_subdirectory(fd_image_stream)
add_subdirectory(remote_logger)

if(CONFIG_FACE_DETECT_UVC_ENABLE)
    add_subdirectory(uvc_stream)
endif()
