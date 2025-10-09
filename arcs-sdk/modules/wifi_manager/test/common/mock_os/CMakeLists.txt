target_sources(${EXECUTABLE_NAME} PRIVATE
    mock_os_ops.c
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PRJ_ROOT}/include/priv/port/os
)

