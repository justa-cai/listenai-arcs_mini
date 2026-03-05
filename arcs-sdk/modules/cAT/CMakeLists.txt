# cAT AT Command Parser - ARCS SDK Integration
# This file should be used instead of the original CMakeLists.txt

if(DEFINED CONFIG_SDK_MODULE_CAT)
    listenai_library_named(cat)

    # Core cAT source
    listenai_library_sources(
        src/cat.c
        port/cat_uart_adapter.c
    )

    # Include directories
    listenai_include_directories(
        src
        port)

    # Configure unsolicited buffer size
    if(DEFINED CONFIG_SDK_MODULE_CAT_UNSOLICITED_BUFFER_SIZE)
        listenai_compile_definitions(
            CAT_UNSOLICITED_CMD_BUFFER_SIZE=${CONFIG_SDK_MODULE_CAT_UNSOLICITED_BUFFER_SIZE}
        )
    endif()

endif()