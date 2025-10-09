if (DEFINED CONFIG_SDK_MODULE_MEMPOOL)
    listenai_library_named(mempool)

    file(GLOB_RECURSE MEMPOOL_SRC "./src/*.c")

    listenai_include_directories(include)
    listenai_library_sources(
        ${MEMPOOL_SRC}
    )
endif()
