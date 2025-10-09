# 添加所有源文件到主库

set(${TARGET_NAME}_GROUP_OCR_SOURCES
    group_ocr.c
)

set(${TARGET_NAME}_GROUP_OCR_PAGE_SOURCES
    pages/ocr_scanner_page.c
)

set(${TARGET_NAME}_GROUP_OCR_WIDGETS_SOURCES
    widgets/lv_scanner.c
)

target_sources(${TARGET_NAME} PRIVATE
    ${${TARGET_NAME}_GROUP_OCR_SOURCES}
    ${${TARGET_NAME}_GROUP_OCR_PAGE_SOURCES}
    ${${TARGET_NAME}_GROUP_OCR_WIDGETS_SOURCES}
)
