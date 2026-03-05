set(LISA_UI_LIB_NAME lisa_ui_app)
set(CMAKE_C_EXTENSIONS ON)
set(CONFIG_LOG_ENABLE 1)
add_definitions(-DCONFIG_LOG_ENABLE=${CONFIG_LOG_ENABLE})

set(CONFIG_LOG_LEVEL 4)
add_definitions(-DCONFIG_LOG_LEVEL=${CONFIG_LOG_LEVEL})

add_library(${LISA_UI_LIB_NAME} STATIC)

target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lisa_ui.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lisa_ui_nav_scr.c
)

target_include_directories(${LISA_UI_LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(portable)
add_subdirectory(apps)
add_subdirectory(assets)
add_subdirectory(lv-i18n)
