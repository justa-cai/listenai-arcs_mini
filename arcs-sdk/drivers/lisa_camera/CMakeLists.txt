
if(CONFIG_LISA_CAMERA_DEVICE)

listenai_library_named(lisa_camera_arcs)

listenai_library_sources(
    lisa_camera_arcs.c
)

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

add_subdirectory(sensors)
add_subdirectory(bus)

endif()
