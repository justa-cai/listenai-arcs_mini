
if (DEFINED CONFIG_LISA_SNTP)
    listenai_library_named(lisa_sntp)
    listenai_library_sources(
        lisa_sntp.c
    )

    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
