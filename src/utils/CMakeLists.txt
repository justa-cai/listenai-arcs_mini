listenai_library_named(utils)

listenai_library_sources(
    aiui_base64.c
    evs_event.c
    evs_pref.c
    evs_utils.c
    evs_uuid.c
    ico_codec.c
    img_jpeg.c
    # ring_buffer.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
