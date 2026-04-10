cmake_minimum_required(VERSION 3.16)
project(mock_netif C)

add_library(mock_netif STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../mock_netif.c
)

set(HAL_TEST_DIR ${CMAKE_CURRENT_LIST_DIR}/../../..)

target_include_directories(mock_netif PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${HAL_TEST_DIR}/utils/fff
)
