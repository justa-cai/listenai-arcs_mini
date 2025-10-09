# 添加所有源文件到主库

set(${TARGET_NAME}_GROUP_SETTING_SOURCES
    group_setting.c
)

set(${TARGET_NAME}_GROUP_SETTING_PAGE_SOURCES
    pages/page_primary.c
    pages/page_item.c
)

target_sources(${TARGET_NAME} PRIVATE
    ${${TARGET_NAME}_GROUP_SETTING_SOURCES}
    ${${TARGET_NAME}_GROUP_SETTING_PAGE_SOURCES}
)
