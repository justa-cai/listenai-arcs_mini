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
#define CH_4G_EN_PIN 0
#define CH_4G_PWR_PIN CH_4G_EN_PIN
#define CH_4G_RST_PIN 1
#define CP_LOG_RX0_PIN 2
#define CP_LOG_TX0_PIN 3
#define A4_SD_D1_PIN 4
#define A5_SD_D0_PIN 5
#define A6_SD_CLK_PIN 6
#define A7_SD_CMD_PIN 7
#define A8_SD_D3_PIN 8
#define A9_SD_D2_PIN 9
#define LCD_PWM_PIN 21
#define LCD_CD_PIN 23
#define LCD_TE_PIN 27
#define I2C1_SDA_PIN 0
#define I2C1_SCL_PIN 1
#define B2_APLOG_TX1_PIN 2
#define POWER_EN_PIN 3
#define POWER_KEY_PIN 4
#define BAT_ADC_PIN 5
#define B6_UART_TX2_PIN 6
#define B7_UART_RX2_PIN 7
#define USB_DET_PIN 8
#define LCD_RST_PIN 9

// GPIO device name aliases
#define CH_4G_EN_DEVICE_NAME "gpioa"
#define CH_4G_PWR_DEVICE_NAME CH_4G_EN_DEVICE_NAME
#define CH_4G_RST_DEVICE_NAME "gpioa"
#define A4_SD_D1_DEVICE_NAME "gpioa"
#define A5_SD_D0_DEVICE_NAME "gpioa"
#define A6_SD_CLK_DEVICE_NAME "gpioa"
#define A7_SD_CMD_DEVICE_NAME "gpioa"
#define A8_SD_D3_DEVICE_NAME "gpioa"
#define A9_SD_D2_DEVICE_NAME "gpioa"
#define LCD_CD_DEVICE_NAME "gpioa"
#define LCD_TE_DEVICE_NAME "gpioa"
#define POWER_EN_DEVICE_NAME "gpiob"
#define POWER_KEY_DEVICE_NAME "gpiob"
#define USB_DET_DEVICE_NAME "gpiob"
#define LCD_RST_DEVICE_NAME "gpiob"

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
