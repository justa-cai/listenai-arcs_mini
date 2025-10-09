if (CONFIG_MODULE_CRC32)
    set(TOP_PATH ${CMAKE_HOME_DIRECTORY})
    set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR})
    file(GLOB_RECURSE LIB_SRCS "*.c")

    listenai_library_named(crc32)
    listenai_library_sources(${LIB_SRCS})
    listenai_include_directories(
        ${LIB_PATH}
        ${TOP_PATH}/build/generated/include
    )
endif()
