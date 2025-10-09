if (CONFIG_MODULE_NOPOLL)
    set(PRJ_PATH ${CMAKE_HOME_DIRECTORY})
    set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR})
    file(GLOB_RECURSE LIB_SRCS "*.c")

    listenai_library_named(nopoll)
    listenai_library_sources(${LIB_SRCS})
    listenai_include_directories(
        ${LIB_PATH}/include
        ${PRJ_PATH}/../../../modules/mbedtls/mbedtls/include
    )

    if (CONFIG_NOPOLL_DELIVER_PONG_FRAME_ENABLED)
        target_compile_definitions(nopoll
            PRIVATE
            -DNOPOLL_DELIVER_PONG_FRAME=1
        )
    endif()

endif()
