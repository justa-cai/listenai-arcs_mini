if (CONFIG_MODULE_ZE_TLS)
    set(PRJ_PATH ${CMAKE_HOME_DIRECTORY})
    set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR})
    file(GLOB_RECURSE LIB_SRCS "*.c")

    listenai_library_named(http_ssl)
    listenai_library_sources(${LIB_SRCS})
    listenai_include_directories(
        ${LIB_PATH}/include
        ${PRJ_PATH}/../../../modules/mbedtls/mbedtls/include
    )

    option(MBEDTLS_API_DEBUG "ze_tls: mbedtls api debug" OFF)
    if (MBEDTLS_API_DEBUG)
        target_compile_definitions(${LISTENAI_CURRENT_LIBRARY}
            PRIVATE MBEDTLS_API_DEBUG
            PRIVATE MBEDTLS_DEBUG_LEVEL=0
        )
    endif()

    target_compile_definitions(${LISTENAI_CURRENT_LIBRARY}
        PRIVATE MBEDTLS_CONNECT_TIMEOUT_MS=3000
        PRIVATE TLS_DEBUG_LEVEL=3
        PRIVATE TLS_RECV_TIMOUT_DEFAULT=9*60*1000
    )

endif()
