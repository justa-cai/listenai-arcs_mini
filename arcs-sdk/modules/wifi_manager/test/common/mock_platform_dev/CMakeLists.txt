target_sources(${EXECUTABLE_NAME} PRIVATE
    mock_platform_dev.c
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fake_nvs
    ${PRJ_ROOT}/include/wifi_manager
)

