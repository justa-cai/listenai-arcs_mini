# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISAUI)

get_filename_component(LISAUI_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
message(STATUS "LISAUI_BASE_DIR: ${LISAUI_BASE_DIR}")
set(TARGET_NAME lisaui_module)
add_library(${TARGET_NAME} STATIC
    ${APP_COMMON_SOURCES}
    ${APP_FRAMEWORK_SOURCES}
    ${APP_USER_SOURCES}
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(core)
add_subdirectory(port)
add_subdirectory(utils)
add_subdirectory(source)
add_subdirectory(user)

if(CONFIG_LVGL_ENV_SIMULATOR)
    set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/port/lisaui.ld)
endif()

if(CONFIG_LISAUI_ENV_ARCS_SDK)
    target_link_libraries(${TARGET_NAME} PUBLIC listenai_interface)
    listenai_link_libraries(${TARGET_NAME})
else()
    target_link_libraries(${TARGET_NAME} PRIVATE -T ${LINKER_SCRIPT} lvgl utils ebus)
endif()

target_compile_features(${TARGET_NAME} PRIVATE c_std_11)
target_link_options(${TARGET_NAME} PRIVATE "-Wl,--whole-archive" "$<TARGET_FILE:lisaui_module>" "-Wl,--no-whole-archive")
set_target_properties(${TARGET_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

endif()
