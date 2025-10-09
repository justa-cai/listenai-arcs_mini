# app/app_standby/CMakeLists.txt
set(LISAUI_APP_TARGET_NAME app_standby)
set(${LISAUI_APP_TARGET_NAME}_SOURCES
    app_standby.c
)

set(${LISAUI_APP_TARGET_NAME}_UI_RESOURCES
    # assets/icons/icon_img_app_store_png.c
    assets/emoji/anim_standby.c
    assets/emoji/anim_wakeup.c
    assets/emoji/anim_speaking2_15fps.c
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
        app_common
        common_widget
        app_alarm
    PUBLIC 
        listenai_interface)
    listenai_link_libraries(${LISAUI_APP_TARGET_NAME})
else()
    target_link_libraries(${LISAUI_APP_TARGET_NAME} 
    PRIVATE
        lvgl
        utils
        common_widget
        app_common
        app_alarm
        )
endif()