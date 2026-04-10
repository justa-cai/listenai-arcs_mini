if(CONFIG_LISA_NET)
    if(CONFIG_SAL_USING_POSIX)
        set(TOP_PATH ${CMAKE_HOME_DIRECTORY})
        set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR})

        listenai_library_named(lisa_net)
        
        set(LIB_SRCS
            "sal/src/netdev.c"
            "sal/src/sal_socket.c"
            "sal/impl/netdev_lwip.c"
        )

        if(CONFIG_LISA_MODEM)
            list(APPEND LIB_SRCS "sal/impl/netdev_at.c")
        endif()

        listenai_library_sources(${LIB_SRCS})

        listenai_include_directories(
            ${LIB_PATH}
            ${LIB_PATH}/include
        )
    endif()
endif()
