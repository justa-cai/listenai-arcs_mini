target_sources(${EXECUTABLE_NAME} PRIVATE
    mock_wifi_ops.c
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

