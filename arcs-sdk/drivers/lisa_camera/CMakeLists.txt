
if(CONFIG_LISA_CAMERA_DEVICE)

listenai_library_named(lisa_camera_drv)

listenai_library_sources_ifdef(CONFIG_SOC_ARCS  lisa_camera.c)
listenai_library_sources_ifdef(CONFIG_SOC_VENUSA  lisa_camera.c)

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

add_subdirectory(sensors)
add_subdirectory(bus)

endif()
