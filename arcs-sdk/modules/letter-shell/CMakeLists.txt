if(DEFINED CONFIG_SDK_MODULE_LETTER_SHELL)
    listenai_library_named(letter-shell)

    file(GLOB
        source_files
        "src/*.c"
    )

    listenai_library_sources(${source_files})
    listenai_include_directories(src)

    if (DEFINED CONFIG_SDK_MODULE_LETTER_SHELL_CONFIG_FILE)
        listenai_compile_definitions(
            SHELL_CFG_USER=CONFIG_SDK_MODULE_LETTER_SHELL_CONFIG_FILE
        )
    endif()


    if(DEFINED CONFIG_SDK_MODULE_LETTER_SHELL_CPP_SUPPORT)
        listenai_include_directories(extensions/cpp_support)
    endif()

    if(DEFINED CONFIG_SDK_MODULE_LETTER_SHELL_FS_SUPPORT)
        file(GLOB
            fs_support_files
            "extensions/fs_support/*.c"
        )
        listenai_library_sources(${fs_support_files})
        listenai_include_directories(extensions/fs_support)
    endif()

    if(DEFINED CONFIG_SDK_MODULE_LETTER_SHELL_ENHANCE)
        file(GLOB
            shell_enhance_files
            "extensions/shell_enhance/*.c"
        )
        listenai_library_sources(${shell_enhance_files})
        listenai_include_directories(extensions/shell_enhance)

    endif()


endif()
