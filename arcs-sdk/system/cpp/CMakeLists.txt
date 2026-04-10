listenai_library_sources(
    sys_cpp.c
)

# C++ runtime linker sections (.init_array, .fini_array, .ctors, .dtors, .eh_frame, etc.)
listenai_add_linker_section(FILE ${CMAKE_CURRENT_SOURCE_DIR}/cpp-runtime-sections.ld SLOT POST_TEXT SORT_KEY "50-system")

if (DEFINED CONFIG_CPP_EXCEPTIONS)
    add_compile_options(
        -fexceptions
    )
endif()
