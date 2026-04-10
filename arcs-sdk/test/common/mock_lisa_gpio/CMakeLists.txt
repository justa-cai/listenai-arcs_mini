cmake_minimum_required(VERSION 3.20)

add_library(mock_lisa_gpio STATIC
    mock_lisa_gpio.c
)

target_include_directories(mock_lisa_gpio PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    $ENV{ARCS_BASE}/drivers/lisa_device
    $ENV{ARCS_BASE}/drivers/lisa_gpio
    $ENV{ARCS_BASE}/modules/fff
)
