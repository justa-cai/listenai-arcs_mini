cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(zig_binding_test)

set(LISTENAI_ZIG_APP ${CMAKE_CURRENT_SOURCE_DIR}/src/zig_app.zig)

listenai_add_executable(${PROJECT_NAME})
listenai_maybe_enable_zig_target(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c)

# 添加驱动头文件路径 (用于 struct sizeof 验证)
target_include_directories(${PROJECT_NAME} PRIVATE
    $ENV{ARCS_BASE}/drivers/lisa_device
    $ENV{ARCS_BASE}/drivers/lisa_gpio
    $ENV{ARCS_BASE}/drivers/lisa_uart
    $ENV{ARCS_BASE}/drivers/lisa_i2c
    $ENV{ARCS_BASE}/drivers/lisa_spi
    $ENV{ARCS_BASE}/drivers/lisa_adc
    $ENV{ARCS_BASE}/drivers/lisa_pwm
    $ENV{ARCS_BASE}/drivers/lisa_flash
    $ENV{ARCS_BASE}/drivers/lisa_display
    $ENV{ARCS_BASE}/drivers/lisa_camera
    $ENV{ARCS_BASE}/drivers/lisa_audio
    $ENV{ARCS_BASE}/drivers/lisa_rtc
    $ENV{ARCS_BASE}/labs/zig/adapter/include
    $ENV{ARCS_BASE}/components/app_player
    $ENV{ARCS_BASE}/modules/httpclient/include
    $ENV{ARCS_BASE}/modules/httpclient/include/API
    $ENV{ARCS_BASE}/modules/http_ssl/include
    $ENV{ARCS_BASE}/modules/mbedtls/mbedtls/include
    $ENV{ARCS_BASE}/modules/mbedtls/configs
    $ENV{ARCS_BASE}/modules/mbedtls/port/mem
    $ENV{ARCS_BASE}/modules/mbedtls/port/platform
    $ENV{ARCS_BASE}/system/heap
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/lwip/lwip-2.2.1/src/include
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/lwip/lwip-2.2.1/contrib/ports/rtos/include
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/lwip/port/include
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/rtos/rtos_al
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/include/net
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/include/wifi
)
