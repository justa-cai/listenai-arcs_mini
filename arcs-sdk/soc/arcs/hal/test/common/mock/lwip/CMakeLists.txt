cmake_minimum_required(VERSION 3.16)
project(mock_lwip C)

add_library(mock_lwip STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../mock_lwip.c
)

set(HAL_TEST_DIR ${CMAKE_CURRENT_LIST_DIR}/../../..)

# Expose include paths for mock headers and lwip shim headers.
target_include_directories(mock_lwip PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${HAL_TEST_DIR}/utils/fff
)
