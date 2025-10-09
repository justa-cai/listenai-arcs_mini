if (DEFINED CONFIG_SDK_MODULE_LVGL)
    SET(SDK_MODULES_LVGL_PATH ${CMAKE_CURRENT_SOURCE_DIR})
    listenai_library_named(modules_lvgl)

    add_subdirectory(porting)
    
    file(GLOB_RECURSE SOURCES 
        ${SDK_MODULES_LVGL_PATH}/src/*.c
        ${SDK_MODULES_LVGL_PATH}/demos/src/*.c
    )
    listenai_library_sources(${SOURCES})

    listenai_include_directories(${SDK_MODULES_LVGL_PATH}/demos)
    listenai_include_directories(${SDK_MODULES_LVGL_PATH}/src)

    listenai_compile_definitions(LV_CONF_INCLUDE_SIMPLE=1 LV_EX_CONF_INCLUDE_SIMPLE=1 LV_LVGL_H_INCLUDE_SIMPLE=1)
endif()
