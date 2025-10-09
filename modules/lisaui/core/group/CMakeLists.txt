# 添加所有源文件到主库
target_sources(${TARGET_NAME} PRIVATE
    group_flow.c
    lisaui_group.c
    lisaui_group_stack.c
    lisaui_group_base.c
)

# 添加该目录到主库的包含路径
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)