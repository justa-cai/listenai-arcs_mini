if(CONFIG_MIDDLEWARE_SIP)

listenai_library_named(sip)

add_definitions(-DSIP_OSAL_FREERTOS)  # 目录作用域：只影响 sip 模块自己的源文件

listenai_library_sources(
    core/sip_g711.c
    core/sip_jpeg_rtp.c
    core/sip_msg.c
    core/sip_sdp.c
    core/sip_rtp.c
    core/sip_ua.c
    ports/sip_osal_freertos.c
    device/sip_jpeg_enc_tables.c
    device/sip_service.c
    device/sip_screen.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/device
    ${CMAKE_CURRENT_SOURCE_DIR}/ports
)

endif()
