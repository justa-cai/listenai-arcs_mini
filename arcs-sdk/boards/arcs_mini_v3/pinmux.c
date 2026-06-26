/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file pinmux.c
 * @brief 引脚复用配置
 *
 * 本文件由 LISA Pinmux Tool 自动生成
 * 生成工具: https://tool.listenai.com/ls-pinmux-tool/
 *
 * 说明:
 * - 所有外设的 pinmux 函数都会生成，未配置的外设函数体为空
 * - 所有函数使用 weak 属性修饰，可在用户代码中重写
 * - 尽量避免手动修改该文件，建议统一使用工具生成
 */

#include "pinmux.h"
#include "IOMuxManager.h"

__attribute__((weak)) void lisa_adc_pinmux()
{
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, BAT_ADC_PIN, 3);
}

__attribute__((weak)) void lisa_capture_pinmux()
{
}

__attribute__((weak)) void lisa_dvp_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 12, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 13, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, 16);
}

__attribute__((weak)) void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CH_4G_EN_PIN, 1);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CH_4G_RST_PIN, 1);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, A4_SD_D1_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, A5_SD_D0_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, A6_SD_CLK_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, A7_SD_CMD_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, A8_SD_D3_PIN, 1);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, A9_SD_D2_PIN, 1);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_CD_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_TE_PIN, 0);
}

__attribute__((weak)) void lisa_gpiob_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, POWER_EN_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, POWER_KEY_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, USB_DET_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_RST_PIN, 0);
}

__attribute__((weak)) void lisa_i2c0_pinmux()
{
}

__attribute__((weak)) void lisa_i2c1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, I2C1_SDA_PIN, 8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, I2C1_SCL_PIN, 8);
}

__attribute__((weak)) void lisa_i2s0_pinmux()
{
}

__attribute__((weak)) void lisa_i2s1_pinmux()
{
}

__attribute__((weak)) void lisa_jtag_pinmux()
{
}

__attribute__((weak)) void lisa_pwm_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_PWM_PIN, 12);
}

__attribute__((weak)) void lisa_qspi_lcd_pinmux()
{
}

__attribute__((weak)) void lisa_rgb_pinmux()
{
}

__attribute__((weak)) void lisa_sdio_pinmux()
{
}

__attribute__((weak)) void lisa_spi0_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, 5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 24, 5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, 5);
}

__attribute__((weak)) void lisa_spi1_pinmux()
{
}

__attribute__((weak)) void lisa_spi2_pinmux()
{
}

__attribute__((weak)) void lisa_uart0_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CP_LOG_RX0_PIN, 2);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CP_LOG_TX0_PIN, 2);
}

__attribute__((weak)) void lisa_uart1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, B2_APLOG_TX1_PIN, 3);
}

__attribute__((weak)) void lisa_uart2_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, B6_UART_TX2_PIN, 4);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, B7_UART_RX2_PIN, 4);
}
