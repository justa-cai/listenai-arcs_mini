# app/app_wifi/CMakeLists.txt
set(LISAUI_APP_TARGET_NAME app_wifi)
set(${LISAUI_APP_TARGET_NAME}_SOURCES
    app_wifi.c
    widget/ls_wifi_list.c
    widget/ls_wifi_connect.c
    widget/ls_wifi_item.c
)

set(${LISAUI_APP_TARGET_NAME}_UI_RESOURCES
    assets/icons/icon_img_app_wifi_png.c
)

# Add special compile flags for ls_wifi_item.c
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/widget/ls_wifi_item.c
    PROPERTIES COMPILE_FLAGS "-fdata-sections -ffunction-sections -O0"
)

add_library(${LISAUI_APP_TARGET_NAME} STATIC
    ${${LISAUI_APP_TARGET_NAME}_SOURCES}
    ${${LISAUI_APP_TARGET_NAME}_UI_RESOURCES}
)

target_include_directories(${LISAUI_APP_TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

if (CONFIG_LISAUI_ENV_ARCS_SDK)
    target_link_libraries(${LISAUI_APP_TARGET_NAME} 
    PRIVATE
        # utils
        lisaui_utils
        app_common
        common_widget
    PUBLIC 
        listenai_interface)
    listenai_link_libraries(${LISAUI_APP_TARGET_NAME})
else()
    target_link_libraries(${LISAUI_APP_TARGET_NAME} 
    PRIVATE
        lvgl
        utils
        lisaui_utils
        common_widget
        app_common
        )
endif()