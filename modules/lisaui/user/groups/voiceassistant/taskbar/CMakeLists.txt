# 添加所有源文件到主库

set(${TARGET_NAME}_GROUP_TASKBAR_SOURCES
    group_taskbar.c
)

set(${TARGET_NAME}_GROUP_TASKBAR_PAGE_SOURCES
    pages/page_nav.c
)


target_sources(${TARGET_NAME} PRIVATE
    ${${TARGET_NAME}_GROUP_TASKBAR_SOURCES}
    ${${TARGET_NAME}_GROUP_TASKBAR_PAGE_SOURCES}
)
