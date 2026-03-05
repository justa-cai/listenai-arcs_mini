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

// Pin aliases
#define CAMERA_RST_PIN 0
#define PA_EN_PIN 1
#define AP_LOG_RX_PIN 2
#define AP_LOG_TX_PIN 3
#define CAM_HSYNC_PIN 10
#define CAM_VSYNC_PIN 11
#define CAM_PCLK_PIN 12
#define CAM_D0_PIN 13
#define CAM_D1_PIN 14
#define CAM_D2_PIN 15
#define CAM_D3_PIN 16
#define CAM_D4_PIN 17
#define CAM_D5_PIN 18
#define CAM_D6_PIN 19
#define CAM_D7_PIN 20
#define LCD_PWM_PIN 21
#define LCD_CS_PIN 22
#define LCD_CD_PIN 23
#define LCD_SPI_DATA_PIN 24
#define LCD_SPI_CLK_PIN 25
#define CAM_MCLK_PIN 26
#define LCD_TE_PIN 27
#define USB_DET_PIN 0
#define LED_PIN 1
#define CP_LOG_TX_PIN 2
#define POWER_EN_PIN 3
#define POWER_KEY_PIN 4
#define BAT_ADC_PIN 5
#define CHARGE_DET_PIN 8
#define LCD_RST_PIN 9

void lisa_adc_pinmux();
void lisa_capture_pinmux();
void lisa_dvp_pinmux();
void lisa_gpioa_pinmux();
void lisa_gpiob_pinmux();
void lisa_i2c0_pinmux();
void lisa_i2c1_pinmux();
void lisa_i2s0_pinmux();
void lisa_i2s1_pinmux();
void lisa_jtag_pinmux();
void lisa_pwm_pinmux();
void lisa_qspi_lcd_pinmux();
void lisa_rgb_pinmux();
void lisa_sdio_pinmux();
void lisa_spi0_pinmux();
void lisa_spi1_pinmux();
void lisa_spi2_pinmux();
void lisa_uart0_pinmux();
void lisa_uart1_pinmux();
void lisa_uart2_pinmux();

#ifdef __cplusplus
}
#endif
