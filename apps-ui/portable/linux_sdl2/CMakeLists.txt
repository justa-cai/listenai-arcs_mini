
set(LV_DRIVERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lv_drivers")
set(LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lvgl")

if(NOT EXISTS "${LV_DRIVERS_DIR}/.git")
    message(STATUS "Cloning lv_drivers repository...")
    execute_process(
        COMMAND git clone https://cloud.listenai.com/CSKG836746/arcs-sdk/modules/lisaui/lv_drivers.git lv_drivers -b release/v8.3
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL "0")
        message(FATAL_ERROR "Failed to clone lv_drivers repository")
    endif()
endif()

if(NOT EXISTS "${LVGL_DIR}/.git")
    message(STATUS "Cloning lvgl repository...")
    execute_process(
        COMMAND git clone https://cloud.listenai.com/CSKG836746/arcs-sdk/modules/lvgl8.git lvgl -b release/v8.4
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL "0")
        message(FATAL_ERROR "Failed to clone lvgl repository")
    endif()
endif()

find_package(SDL2 REQUIRED SDL2)

include_directories(
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}/../
)

target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    lisa_ui_lvgl.c
    lisa_ui_mem.c
    lisa_ui_invoke.c
)

target_include_directories(${LISA_UI_LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

include_directories(./)

set(CONFIG_LVGL_ENV_SIMULATOR 1)
add_definitions(-DCONFIG_LVGL_ENV_SIMULATOR=${CONFIG_LVGL_ENV_SIMULATOR})

set(LV_CONF_PATH lv_conf.h)

add_definitions(-DLV_CONF_PATH=${LV_CONF_PATH})

target_link_libraries(
    ${LISA_UI_LIB_NAME}
    PUBLIC
    lv_drivers
    ${SDL2_LIBRARIES}
    -lm
    -lpthread
    -ldl
)

add_subdirectory(lvgl)
add_subdirectory(lv_drivers)
