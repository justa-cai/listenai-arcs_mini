if (CONFIG_MODULE_HTTPCLIENT)
    set(PRJ_PATH ${CMAKE_HOME_DIRECTORY})
    set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR})
    file(GLOB_RECURSE LIB_SRCS "*.c")

    listenai_library_named(httpc)
    listenai_library_sources(${LIB_SRCS})
    listenai_include_directories(
        ${LIB_PATH}/include
        ${LIB_PATH}/include/API
        ${PRJ_PATH}/../../../modules/mbedtls/mbedtls/include
        ${PRJ_PATH}/../../../modules/http_ssl/include
    )

    option(HTTPS "httpclient: HTTPS support" ON)
    option(HTTPCLIENT_DEBUG "httpclient: Debug support" OFF)

    if (HTTPS)
        target_compile_definitions(${LISTENAI_CURRENT_LIBRARY}
            PRIVATE HTTPC_SSL
        )
    endif()

    if (HTTPCLIENT_DEBUG)
        target_compile_definitions(${LISTENAI_CURRENT_LIBRARY}
            PRIVATE HTTPCLIENT_DEBUG
        )
    endif()

endif()
