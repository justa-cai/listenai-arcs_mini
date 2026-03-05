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
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_HSYNC_PIN, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_VSYNC_PIN, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_PCLK_PIN, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D0_PIN, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D1_PIN, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D2_PIN, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D3_PIN, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D4_PIN, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D5_PIN, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D6_PIN, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D7_PIN, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_MCLK_PIN, 16);
}

__attribute__((weak)) void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAMERA_RST_PIN, 1);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, PA_EN_PIN, 1);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_CD_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_TE_PIN, 0);
}

__attribute__((weak)) void lisa_gpiob_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, USB_DET_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LED_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, POWER_EN_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, POWER_KEY_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, CHARGE_DET_PIN, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_RST_PIN, 0);
}

__attribute__((weak)) void lisa_i2c0_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, 8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, 8);
}

__attribute__((weak)) void lisa_i2c1_pinmux()
{
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
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_CS_PIN, 5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_SPI_DATA_PIN, 5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_SPI_CLK_PIN, 5);
}

__attribute__((weak)) void lisa_spi1_pinmux()
{
}

__attribute__((weak)) void lisa_spi2_pinmux()
{
}

__attribute__((weak)) void lisa_uart0_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, AP_LOG_RX_PIN, 2);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, AP_LOG_TX_PIN, 2);
}

__attribute__((weak)) void lisa_uart1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, CP_LOG_TX_PIN, 3);
}

__attribute__((weak)) void lisa_uart2_pinmux()
{
}

