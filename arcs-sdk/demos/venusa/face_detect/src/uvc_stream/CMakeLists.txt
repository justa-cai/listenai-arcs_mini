listenai_library_named(uvc_stream)

listenai_library_sources(
    uvc_stream.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ARCS_SDK_BASE}/modules/chryusb
)
