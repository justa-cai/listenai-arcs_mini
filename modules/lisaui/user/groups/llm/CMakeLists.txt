add_subdirectory(launcher)

# 添加该目录到主库的包含路径
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)