if(CONFIG_MIDDLEWARE_SIP)

listenai_library_named(sip)

listenai_library_sources(
    core/sip_g711.c
    ports/sip_osal_freertos.c
    device/sip_service.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ports
)

endif()
