cmake_minimum_required(VERSION 3.16)
project(mock_rtos C)

add_library(mock_rtos STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../mock_rtos.c
)

set(HAL_TEST_DIR ${CMAKE_CURRENT_LIST_DIR}/../../..)

target_include_directories(mock_rtos PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${HAL_TEST_DIR}/utils/fff
)
