cmake_minimum_required(VERSION 3.10)

if(CONFIG_ARCS_HAL_BTOS)
    # 静态库目标
    listenai_library_named(arcs_hal_btos)

    listenai_library_sources(
        btos_al.c
        task/bt_os_task.c
    )

    if (CONFIG_BT_AUDIO)
        listenai_library_sources(
            task/aud_os_task.c
            task/aud_pro_os_task.c
        )
    endif()

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )

    # 包含目录配置
    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/task/api/
    )
endif()
