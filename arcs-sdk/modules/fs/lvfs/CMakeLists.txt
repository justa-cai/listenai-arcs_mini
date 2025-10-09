if(CONFIG_LVFS)

set(LIBRARY_NAME lvfs)
add_library(${LIBRARY_NAME} STATIC "")

if(CONFIG_LVFS_POSIX_LARGE64_FILES)
add_definitions(-D__LARGE64_FILES)
endif()

target_include_directories(${LIBRARY_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 基本源文件
set(LSFS_SOURCES
    lvfs.c
)

if (CONFIG_LVFS_POSIX_API)

list(APPEND LSFS_SOURCES
posix/fs.c
)
target_include_directories(${LIBRARY_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

endif()

target_sources(${LIBRARY_NAME} PRIVATE 
    ${LSFS_SOURCES}
)

target_link_libraries(${LIBRARY_NAME} PRIVATE fs_env)

if(DEFINED LISTENAI_CMAKE_PATH)
listenai_link_libraries(${LIBRARY_NAME})
endif()
endif() # CONFIG_LSFS