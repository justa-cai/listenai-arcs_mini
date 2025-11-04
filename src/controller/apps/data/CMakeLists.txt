# 添加该目录到主库的包含路径
listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

listenai_library_sources(
    ./lisaui_user_data.c
    ./lisaui_videoqueue.c
)
