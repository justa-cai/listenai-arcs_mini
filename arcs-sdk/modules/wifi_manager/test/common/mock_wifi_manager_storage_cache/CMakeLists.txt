target_sources(${EXECUTABLE_NAME} PRIVATE
    mock_wifi_manager_storage_cache.c
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PRJ_ROOT}/include/wifi_manager
    ${PRJ_ROOT}/src
)
