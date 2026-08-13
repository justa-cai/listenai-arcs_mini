/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file pinmux.h
 * @brief 引脚复用配置头文件
 *
 * 本文件由 LISA Pinmux Tool 自动生成
 * 生成工具: https://tool.listenai.com/ls-pinmux-tool/
 *
 * 说明:
 * - 包含所有外设 pinmux 函数的声明
 * - 包含引脚别名宏定义（如果有设置）
 * - 尽量避免手动修改该文件，建议统一使用工具生成
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PA_EN_PIN 0
#define CAMERA_DVP_SDA_PIN  1
#define CAMERA_DVP_SCL_PIN  2

void lisa_capture_pinmux();
void lisa_adc_pinmux();
void lisa_dmic_pinmux();
void lisa_dvp_pinmux();
void lisa_gpioa_pinmux();
void lisa_gpiob_pinmux();
void lisa_i2c0_pinmux();
void lisa_i2c1_pinmux();
void lisa_i2s0_pinmux();
void lisa_i2s1_pinmux();
void lisa_i8080_pinmux();
void lisa_ir_pinmux();
void lisa_jtag_pinmux();
void lisa_pwm_pinmux();
void lisa_qspi_lcd_pinmux();
void lisa_sdio_pinmux();
void lisa_spi0_pinmux();
void lisa_spi1_pinmux();
void lisa_uart0_pinmux();
void lisa_uart1_pinmux();
void lisa_uart2_pinmux();

#ifdef __cplusplus
}
#endif
