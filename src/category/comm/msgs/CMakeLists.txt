listenai_library_named(events)

listenai_library_sources(
    voice_wifi_msg.c
    voice_cloud_msg.c
    voice_wakeup_msg.c
    voice_platform.c
    voice_rcmd_msg.c
)

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
