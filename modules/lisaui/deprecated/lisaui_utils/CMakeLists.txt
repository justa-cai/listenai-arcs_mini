if (CONFIG_LVGL_ENV_SIMULATOR)
    add_library(lisaui_utils STATIC
        img_utils.c
        lv_img_utils.c
        cJSON.c
    )
else()
    add_library(lisaui_utils STATIC
        img_utils.c
        lv_img_utils.c
    )
endif()



target_include_directories(lisaui_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

if (CONFIG_LISAUI_ENV_ARCS_SDK)
    target_link_libraries(lisaui_utils
    PRIVATE
    PUBLIC
        listenai_interface)
    listenai_link_libraries(lisaui_utils)
else()
    target_link_libraries(lisaui_utils
    PRIVATE
        lvgl
        utils
        app_common
        )
endif()