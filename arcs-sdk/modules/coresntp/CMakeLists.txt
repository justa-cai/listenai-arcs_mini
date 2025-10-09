if (CONFIG_MODULE_CORESNTP)
    set(PRJ_PATH ${CMAKE_HOME_DIRECTORY})
    set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR})

    set (LIB_SRCS
        ${LIB_PATH}/source/core_sntp_client.c
        ${LIB_PATH}/source/core_sntp_serializer.c
    )

    listenai_library_named(coresntp)
    listenai_library_sources(${LIB_SRCS})
    listenai_include_directories(
        ${LIB_PATH}/source/include
        ${LIB_PATH}/../../modules/mbedtls/mbedtls/include
    )

    option(SNTP_DO_NOT_USE_CUSTOM_CONFIG "coreSNTP: build the SNTP library without a custom config" ON)
    if (SNTP_DO_NOT_USE_CUSTOM_CONFIG)
        target_compile_definitions(${LISTENAI_CURRENT_LIBRARY}
            PRIVATE SNTP_DO_NOT_USE_CUSTOM_CONFIG
        )
    endif()


endif()
