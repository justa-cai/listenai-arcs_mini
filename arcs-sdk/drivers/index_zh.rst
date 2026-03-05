.. _drivers:

设备驱动
========

ARCS SDK 设备驱动基于 LISA 轻量级设备框架，提供统一的设备抽象层，实现自动注册、状态管理和访问控制。
驱动层自动处理引脚复用配置，应用层通过统一的设备 API 访问 GPIO、UART、ADC、定时器等外设，无需关心底层硬件细节。

.. toctree::
    :maxdepth: 1

    lisa_pinmux/README.md
    lisa_gpio/README.md
    lisa_adc/README.md
    lisa_uart/README.md
    lisa_hwtimer/README.md
    lisa_flash/README.md
    lisa_sdmmc/README.md
    lisa_spi/README.md
    lisa_qspilcd/README.md
    lisa_display/README.md
    lisa_pwm/README.md
    lisa_rtc/README.md
    lisa_wdt/README.md
    lisa_audio/README.md
    lisa_i2c/README.md
    lisa_touch/README.md
    lisa_dvp/README.md
    lisa_camera/README.md
    lisa_rgb/README.md

