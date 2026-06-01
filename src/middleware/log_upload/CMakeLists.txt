if(CONFIG_LOG_UPLOAD)

listenai_library_named(log_upload)

listenai_library_sources(
    log_buffer.c
    log_upload.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

endif()
