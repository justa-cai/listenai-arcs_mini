# 添加所有源文件到主库
target_sources(${TARGET_NAME} PRIVATE
    lisaui_manager.c
)

# 添加该目录到主库的包含路径
listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)