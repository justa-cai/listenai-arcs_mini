if(CONFIG_LSFS)

set(LIBRARY_NAME lsfs)
add_library(${LIBRARY_NAME} STATIC "")

target_include_directories(${LIBRARY_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 基本源文件
set(LSFS_SOURCES
    lsfs.c
)

# 当LSFS_FAT开启时添加FAT相关源文件
if(CONFIG_LSFS_FAT)
    list(APPEND LSFS_SOURCES
        lsfs_fat.c
    )
endif()

# 支持VFS
if(CONFIG_LSFS_REGISTER_VFS)
    list(APPEND LSFS_SOURCES
        lsfs_vfs.c
    )
endif()

target_sources(${LIBRARY_NAME} PRIVATE 
    ${LSFS_SOURCES}
)

# 基础依赖
target_link_libraries(${LIBRARY_NAME} PUBLIC disk)

# 如果启用了FATFS，添加elmfat依赖
if(CONFIG_LSFS_FAT)
    target_link_libraries(${LIBRARY_NAME} PRIVATE elmfat)
endif()

if (CONFIG_LSFS_REGISTER_VFS)
target_link_libraries(${LIBRARY_NAME} PRIVATE lvfs)
endif()

if(DEFINED LISTENAI_CMAKE_PATH)
listenai_link_libraries(${LIBRARY_NAME})
endif()

endif() # CONFIG_LSFS