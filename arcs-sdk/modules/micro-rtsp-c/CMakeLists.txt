if(DEFINED CONFIG_SDK_MODULE_MICRO_RTSP_C)

listenai_psram_library_named(micro_rtsp_c)

listenai_library_sources(
    src/linked_list.c
    src/session.c
    src/streamer.c)

listenai_include_directories(
    src/)

endif()