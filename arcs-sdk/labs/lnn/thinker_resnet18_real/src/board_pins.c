/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <IOMuxManager.h>
#include <board.h>

#include "lnn_resnet18_real_cp.h"

#ifdef CONFIG_BOARD_ARCS_EVB

void lisa_gpioa_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
    IOMuxManager_PinConfigure(TOUCH_RST_PORT, TOUCH_RST_PIN, TOUCH_RST_FUNC);
    IOMuxManager_PinConfigure(TOUCH_INT_PORT, TOUCH_INT_PIN, TOUCH_INT_FUNC);
}

void lisa_gpiob_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_TE_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CD_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, CAM_PWDN_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_i2c0_pinmux(void)
{
    IOMuxManager_PinConfigure(TOUCH_I2C_SDA_PORT, TOUCH_I2C_SDA_PIN, TOUCH_I2C_SDA_FUNC);
    IOMuxManager_PinConfigure(TOUCH_I2C_SCL_PORT, TOUCH_I2C_SCL_PIN, TOUCH_I2C_SCL_FUNC);
}

void lisa_spi1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER6);
}

void lisa_pwm_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_PWM_PIN, CSK_IOMUX_FUNC_ALTER12);
}

void lisa_dvp_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_HSYNC_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_VSYNC_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_PCLK_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D0_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D1_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D2_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D3_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D4_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D5_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D6_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D7_PIN, CSK_IOMUX_FUNC_ALTER16);
}

#endif
