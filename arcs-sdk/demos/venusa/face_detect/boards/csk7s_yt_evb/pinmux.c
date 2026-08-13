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
}

__attribute__((weak)) void lisa_capture_pinmux()
{
}

__attribute__((weak)) void lisa_dmic_pinmux()
{
}

__attribute__((weak)) void lisa_dvp_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_ALTER13); //HSYNC
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, CSK_IOMUX_FUNC_ALTER13); //VSYNC
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, CSK_IOMUX_FUNC_ALTER13); //PCLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, CSK_IOMUX_FUNC_ALTER13);  //MCLK

    /* D0 ~ D7 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 24, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 27, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 28, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 29, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 30, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 31, CSK_IOMUX_FUNC_ALTER13);
}

__attribute__((weak)) void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, PA_EN_PIN, CSK_IOMUX_FUNC_ALTER1);
}

__attribute__((weak)) void lisa_gpiob_pinmux()
{
}

__attribute__((weak)) void lisa_i2c0_pinmux()
{

}

__attribute__((weak)) void lisa_i2c1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, CAMERA_DVP_SCL_PIN, CSK_IOMUX_FUNC_ALTER8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, CAMERA_DVP_SDA_PIN, CSK_IOMUX_FUNC_ALTER8);
}

__attribute__((weak)) void lisa_i2s0_pinmux()
{
}

__attribute__((weak)) void lisa_i2s1_pinmux()
{
}

__attribute__((weak)) void lisa_i8080_pinmux()
{
}

__attribute__((weak)) void lisa_ir_pinmux()
{
}

__attribute__((weak)) void lisa_jtag_pinmux()
{
}

__attribute__((weak)) void lisa_pwm_pinmux()
{
}

__attribute__((weak)) void lisa_qspi_lcd_pinmux()
{
}

__attribute__((weak)) void lisa_sdio_pinmux()
{
}

__attribute__((weak)) void lisa_spi0_pinmux()
{
}

__attribute__((weak)) void lisa_spi1_pinmux()
{
}

__attribute__((weak)) void lisa_uart0_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, 2);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, 2);
}

__attribute__((weak)) void lisa_uart1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7, CSK_IOMUX_FUNC_ALTER3);
}

__attribute__((weak)) void lisa_uart2_pinmux()
{
}
