if (DEFINED CONFIG_SDK_MODULE_CURL)

    set(CURL_LIBRARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/curl)
    set(CURL_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/configs)
    set(CURL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/port)

    listenai_library_named(curl)

    file(GLOB_RECURSE CURL_LIB_SRCS "${CURL_LIBRARY_DIR}/lib/*.c")
    list(APPEND CURL_LIB_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/port/easy.c")
    list(APPEND CURL_LIB_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/port/timeval.c")
    list(APPEND CURL_LIB_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/port/mem/curl_mem.c")

    list(REMOVE_ITEM CURL_LIB_SRCS
        "${CURL_LIBRARY_DIR}/lib/easy.c"
        "${CURL_LIBRARY_DIR}/lib/timeval.c"
        )

    listenai_library_sources(${CURL_LIB_SRCS})

    listenai_include_directories(
        ${CURL_LIBRARY_DIR}/include
        ${CURL_LIBRARY_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/port/mem/
    )

    listenai_include_directories(${CURL_CONFIG_DIR})

    add_definitions(-DHAVE_CONFIG_H)
endif()
