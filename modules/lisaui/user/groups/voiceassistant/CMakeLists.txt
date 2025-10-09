add_subdirectory(launcher)
add_subdirectory(setting)
add_subdirectory(taskbar)

# 添加该目录到主库的包含路径
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)