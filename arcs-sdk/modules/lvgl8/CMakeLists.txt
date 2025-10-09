cmake_minimum_required(VERSION 3.12.4)

set(LVGL_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

if (DEFINED CONFIG_SDK_MODULE_LVGL8)
    SET(SDK_MODULES_LVGL_PATH ${CMAKE_CURRENT_SOURCE_DIR})
    listenai_library_named(modules_lvgl8)

    listenai_compile_definitions(LV_CONF_INCLUDE_SIMPLE=1 LV_EX_CONF_INCLUDE_SIMPLE=1 LV_LVGL_H_INCLUDE_SIMPLE=1)

    add_subdirectory(porting)

    listenai_include_directories(${LVGL_ROOT_DIR}/src)
    listenai_include_directories(${LVGL_ROOT_DIR})
    
    file(GLOB_RECURSE SOURCES ${LVGL_ROOT_DIR}/src/*.c)
    
    listenai_library_sources(${SOURCES})

    if (CONFIG_LV_BUILD_EXAMPLES)
        listenai_include_directories(${LVGL_ROOT_DIR}/demos)
        listenai_include_directories(${LVGL_ROOT_DIR}/examples)

        file(GLOB_RECURSE EXAMPLE_SOURCES ${LVGL_ROOT_DIR}/examples/*.c)
        file(GLOB_RECURSE DEMO_SOURCES ${LVGL_ROOT_DIR}/demos/*.c)

        listenai_library_sources(${EXAMPLE_SOURCES})
        listenai_library_sources(${DEMO_SOURCES})
    endif()

endif()
