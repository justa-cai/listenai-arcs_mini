if(CONFIG_ARCS_HAL_EVENT)
    # 静态库目标
    listenai_library_named(event)

    # 源文件配置
    file(GLOB_RECURSE SOURCES
        "*.c"
    )

    listenai_library_sources(${SOURCES})

    # 包含目录配置
    listenai_include_directories(
        ../../../include/CMSIS
        ../../../include/CMSIS/core
    )

    # 编译定义
    target_compile_definitions(event PRIVATE CFG_EVENT=1)

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )
endif()
