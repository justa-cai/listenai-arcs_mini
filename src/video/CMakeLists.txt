# Video module only build when camera component is enabled
if(CONFIG_COMPONENT_CAMERA)

listenai_library_named(video)

listenai_library_sources(
    video_camera.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

endif()
