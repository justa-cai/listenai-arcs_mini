listenai_library_sources(sensor.c)

listenai_library_sources_ifdef(CONFIG_LISA_CAMERA_SENSOR_BF3901 src/bf3901.c)
listenai_library_sources_ifdef(CONFIG_LISA_CAMERA_SENSOR_GC032A src/gc032a.c)
listenai_library_sources_ifdef(CONFIG_LISA_CAMERA_SENSOR_GC0328 src/gc0328.c)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)