listenai_library_sources(sensor.c)

listenai_library_sources_ifdef(CONFIG_BF3901_SUPPORT src/bf3901.c)
listenai_library_sources_ifdef(CONFIG_GC032A_SUPPORT src/gc032a.c)
listenai_library_sources_ifdef(CONFIG_GC0328_SUPPORT src/gc0328.c)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)