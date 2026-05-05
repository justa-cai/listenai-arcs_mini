if(DEFINED CONFIG_SDK_MODULE_MICRO_RTSP_C)

listenai_psram_library_named(micro_rtsp_c)

listenai_library_sources(
    src/linked_list.c
    src/rtsp_rtp_timestamp_math.c
    src/rtsp_streamer_timing.c
    src/session_send_policy.c
    src/streamer_transport_policy.c
    src/session.c
    src/streamer.c)

listenai_include_directories(
    src/)

endif()
