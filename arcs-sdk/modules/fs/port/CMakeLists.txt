# 创建port接口库
add_library(fs_env STATIC)

# 添加头文件路径
target_include_directories(fs_env
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(fs_env
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/arcs/fs_env.c
)

if(CONFIG_FS_ENV_OS_FREERTOS)
target_link_libraries(fs_env PUBLIC ${CONFIG_FS_ENV_OS_LIBRARY_NAME})
endif()

# 导出库供其他模块使用
add_library(fs::env ALIAS fs_env)