# app/app_common/CMakeLists.txt
set(LISAUI_COMMON_TARGET_NAME app_common)
set(APP_FRAMEWORK_SOURCES
    lisaui_app_manager.c
    lisaui_view_manager.c
)

if(CONFIG_LISAUI_DBUS_ENABLE)
    list(APPEND APP_FRAMEWORK_SOURCES
        lisaui_dbus.c
    )
endif()

set(APP_SOURCES
    lisaui_app_common.c
    lisaui_asset_manager.c
)

add_library(${LISAUI_COMMON_TARGET_NAME} STATIC
    ${APP_SOURCES}
    ${APP_FRAMEWORK_SOURCES}
)

target_include_directories(${LISAUI_COMMON_TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/
)

if (CONFIG_LISAUI_ENV_ARCS_SDK)
    target_link_libraries(${LISAUI_COMMON_TARGET_NAME}
        PRIVATE
            # lvgl
            # utils
            lisaui_utils
            common_widget
        PUBLIC
            listenai_interface)
    listenai_link_libraries(${LISAUI_COMMON_TARGET_NAME})
else()
    target_link_libraries(${LISAUI_COMMON_TARGET_NAME}
        PRIVATE
            lvgl
            utils
            lisaui_utils
            common_widget
            )
endif()