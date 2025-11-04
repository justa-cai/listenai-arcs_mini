# 添加该目录到主库的包含路径

target_sources(${TARGET_NAME} PRIVATE
    lv_img_utils.c
    img_utils.c
    lv_port_file.c
)
listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)