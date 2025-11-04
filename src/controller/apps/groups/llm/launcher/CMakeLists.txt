# 添加所有源文件到主库

set(${TARGET_NAME}_GROUP_LAUNCHER_SOURCES
    group_launcher.c
)

set(${TARGET_NAME}_GROUP_LAUNCHER_PAGE_SOURCES
    pages/page_primary.c
    pages/page_info.c
)

listenai_library_sources(
    ${${TARGET_NAME}_GROUP_LAUNCHER_SOURCES}
    ${${TARGET_NAME}_GROUP_LAUNCHER_PAGE_SOURCES}
)
