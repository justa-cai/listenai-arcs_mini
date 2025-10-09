# app/app_weather/CMakeLists.txt
set(LISAUI_APP_TARGET_NAME app_weather)

set(${LISAUI_APP_TARGET_NAME}_SOURCES
    app_weather.c
)

set(${LISAUI_APP_TARGET_NAME}_UI_RESOURCES
    # assets/icons/icon_img_app_store_png.c
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