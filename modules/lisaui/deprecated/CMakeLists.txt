# SPDX-License-Identifier: Apache-2.0
# project(lisaui C)
get_filename_component(LISAUI_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
message(STATUS "LISAUI_BASE_DIR: ${LISAUI_BASE_DIR}")

set(CONFIG_LISAUI_ENABLE 1)
add_definitions(-DCONFIG_LISAUI_ENABLE=${CONFIG_LISAUI_ENABLE})

set(APP_COMMON_SOURCES
    ui.c
)

add_subdirectory(lisaui_utils)
add_subdirectory(widgets)
add_subdirectory(app_common)

add_library(lisaui STATIC
    ${APP_COMMON_SOURCES}
    ${APP_FRAMEWORK_SOURCES}
    ${APP_USER_SOURCES}
)

target_include_directories(lisaui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/app_common
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
)

file(GLOB APP_DIRS "app/*")
foreach(APP_DIR ${APP_DIRS})
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/app/" "" REL_APP_DIR ${APP_DIR})

    if(REL_APP_DIR MATCHES "^app_" OR REL_APP_DIR MATCHES "^psram_app_")
        if(IS_DIRECTORY ${APP_DIR} AND EXISTS "${APP_DIR}/.skip_build")
            message(STATUS "lisaui: Skipping directory (skip file): app/${REL_APP_DIR}")
        else()
            # message(STATUS "Adding application directory: app/${REL_APP_DIR}")
            # add_subdirectory(${APP_DIR})
            if(IS_DIRECTORY ${APP_DIR} AND EXISTS "${APP_DIR}/CMakeLists.txt")
                message(STATUS "lisaui: Adding application directory: app/${REL_APP_DIR}")
                add_subdirectory(${APP_DIR})
            else()
                message(STATUS "lisaui: Skipping directory (no CMakeLists.txt): app/${REL_APP_DIR}")
            endif()
        endif()
    endif()
endforeach()

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/lisaui_apps.ld)

if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-map,${LINKER_SCRIPT}")
else()
    # target_link_options(lisaui PRIVATE -T ${LINKER_SCRIPT})
    get_property(ADDED_SUBDIRS DIRECTORY PROPERTY SUBDIRECTORIES)
    set(APP_LIBRARIES "")
endif()

foreach(SUBDIR ${ADDED_SUBDIRS})
    get_filename_component(SUBDIR_NAME ${SUBDIR} NAME)
    if(SUBDIR_NAME MATCHES "^app_")
        list(APPEND APP_LIBRARIES ${SUBDIR_NAME})
    endif()
    if(SUBDIR_NAME MATCHES "^psram_app_")
        list(APPEND APP_LIBRARIES ${SUBDIR_NAME})
    endif()
endforeach()

message(STATUS "lisaui: APP_LIBRARIES: ${APP_LIBRARIES}")

if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-map,${LINKER_SCRIPT}")
    target_link_libraries(lisaui PRIVATE
    ${APP_LIBRARIES}
    # -unity
    lvgl
    utils
    app_common
    common_widget
    )
else()
    if(CONFIG_LISAUI_ENV_ARCS_SDK)
        target_link_libraries(lisaui
        PRIVATE
            # -T ${LINKER_SCRIPT}
            ${APP_LIBRARIES}
            # -unity
            # lvgl
            # utils
            app_common
            common_widget
        PUBLIC 
            listenai_interface
        )
        listenai_link_libraries(lisaui)
    else()
        target_link_libraries(lisaui
        PRIVATE
            -T ${LINKER_SCRIPT}
            ${APP_LIBRARIES}
            # -unity
            # lvgl
            utils
            app_common
            common_widget
        PUBLIC
        )
    endif()
endif()

target_compile_features(lisaui PRIVATE c_std_11)

set_target_properties(lisaui PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)