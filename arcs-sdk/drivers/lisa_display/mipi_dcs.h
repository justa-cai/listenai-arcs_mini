/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * @file mipi_dcs.h
 * @brief MIPI Display Command Set (DCS) 标准命令定义
 * 
 * 这些命令是大部分LCD控制器芯片共用的标准命令集。
 * 参考: MIPI Alliance Specification for Display Command Set
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 *  MIPI DCS 标准命令定义
 * ======================================================================== */

/* --- System Commands --- */
#define MIPI_DCS_NOP                    0x00  /* No Operation */
#define MIPI_DCS_SOFT_RESET             0x01  /* Software Reset */
#define MIPI_DCS_GET_DISPLAY_ID         0x04  /* Get Display ID */
#define MIPI_DCS_GET_ERROR_COUNT        0x05  /* Get Error Count on DSI */
#define MIPI_DCS_GET_RED_CHANNEL        0x06  /* Get Red Channel */
#define MIPI_DCS_GET_GREEN_CHANNEL      0x07  /* Get Green Channel */
#define MIPI_DCS_GET_BLUE_CHANNEL       0x08  /* Get Blue Channel */
#define MIPI_DCS_GET_DISPLAY_STATUS     0x09  /* Get Display Status */
#define MIPI_DCS_GET_POWER_MODE         0x0A  /* Get Power Mode */
#define MIPI_DCS_GET_ADDRESS_MODE       0x0B  /* Get Address Mode */
#define MIPI_DCS_GET_PIXEL_FORMAT       0x0C  /* Get Pixel Format */
#define MIPI_DCS_GET_DISPLAY_MODE       0x0D  /* Get Display Mode */
#define MIPI_DCS_GET_SIGNAL_MODE        0x0E  /* Get Signal Mode */
#define MIPI_DCS_GET_DIAGNOSTIC_RESULT  0x0F  /* Get Diagnostic Result */

/* --- Display Control Commands --- */
#define MIPI_DCS_ENTER_SLEEP_MODE       0x10  /* Enter Sleep Mode */
#define MIPI_DCS_EXIT_SLEEP_MODE        0x11  /* Sleep Out */
#define MIPI_DCS_ENTER_PARTIAL_MODE     0x12  /* Partial Mode On */
#define MIPI_DCS_ENTER_NORMAL_MODE      0x13  /* Normal Display Mode On */
#define MIPI_DCS_EXIT_INVERT_MODE       0x20  /* Display Inversion Off */
#define MIPI_DCS_ENTER_INVERT_MODE      0x21  /* Display Inversion On */
#define MIPI_DCS_SET_GAMMA_CURVE        0x26  /* Gamma Set */
#define MIPI_DCS_SET_DISPLAY_OFF        0x28  /* Display Off */
#define MIPI_DCS_SET_DISPLAY_ON         0x29  /* Display On */

/* --- Memory Access Commands --- */
#define MIPI_DCS_SET_COLUMN_ADDRESS     0x2A  /* Column Address Set */
#define MIPI_DCS_SET_PAGE_ADDRESS       0x2B  /* Page Address Set (Row) */
#define MIPI_DCS_WRITE_MEMORY_START     0x2C  /* Memory Write */
#define MIPI_DCS_WRITE_LUT              0x2D  /* Color Set (LUT) */
#define MIPI_DCS_READ_MEMORY_START      0x2E  /* Memory Read */

#define MIPI_DCS_SET_PARTIAL_ROWS       0x30  /* Partial Area */
#define MIPI_DCS_SET_PARTIAL_COLUMNS    0x31  /* Partial Columns */
#define MIPI_DCS_SET_SCROLL_AREA        0x33  /* Vertical Scrolling Definition */
#define MIPI_DCS_SET_TEAR_OFF           0x34  /* Tearing Effect Line OFF */
#define MIPI_DCS_SET_TEAR_ON            0x35  /* Tearing Effect Line ON */
#define MIPI_DCS_SET_ADDRESS_MODE       0x36  /* Memory Access Control (MADCTL) */
#define MIPI_DCS_SET_SCROLL_START       0x37  /* Vertical Scrolling Start Address */
#define MIPI_DCS_EXIT_IDLE_MODE         0x38  /* Idle Mode Off */
#define MIPI_DCS_ENTER_IDLE_MODE        0x39  /* Idle Mode On */
#define MIPI_DCS_SET_PIXEL_FORMAT       0x3A  /* Interface Pixel Format */

#define MIPI_DCS_WRITE_MEMORY_CONTINUE  0x3C  /* Memory Write Continue */
#define MIPI_DCS_READ_MEMORY_CONTINUE   0x3E  /* Memory Read Continue */

#define MIPI_DCS_SET_TEAR_SCANLINE      0x44  /* Set Tear Scanline */
#define MIPI_DCS_GET_SCANLINE           0x45  /* Get Scanline */

/* --- Brightness Control --- */
#define MIPI_DCS_WRITE_DISPLAY_BRIGHTNESS 0x51  /* Write Display Brightness */
#define MIPI_DCS_READ_DISPLAY_BRIGHTNESS  0x52  /* Read Display Brightness */
#define MIPI_DCS_WRITE_CTRL_DISPLAY       0x53  /* Write CTRL Display */
#define MIPI_DCS_READ_CTRL_DISPLAY        0x54  /* Read CTRL Display */
#define MIPI_DCS_WRITE_CABC               0x55  /* Write Content Adaptive Brightness Control */
#define MIPI_DCS_READ_CABC                0x56  /* Read Content Adaptive Brightness Control */
#define MIPI_DCS_WRITE_CABC_MIN           0x5E  /* Write CABC Minimum Brightness */
#define MIPI_DCS_READ_CABC_MIN            0x5F  /* Read CABC Minimum Brightness */

/* --- Misc Commands --- */
#define MIPI_DCS_READ_DDB_START         0xA1  /* Read DDB Start */
#define MIPI_DCS_READ_DDB_CONTINUE      0xA8  /* Read DDB Continue */

/* ========================================================================
 *  常用命令别名（为了兼容性和可读性）
 * ======================================================================== */

/* Sleep Control */
#define LCD_CMD_SLEEP_IN                MIPI_DCS_ENTER_SLEEP_MODE
#define LCD_CMD_SLEEP_OUT               MIPI_DCS_EXIT_SLEEP_MODE

/* Display Control */
#define LCD_CMD_DISPLAY_OFF             MIPI_DCS_SET_DISPLAY_OFF
#define LCD_CMD_DISPLAY_ON              MIPI_DCS_SET_DISPLAY_ON
#define LCD_CMD_INVERT_OFF              MIPI_DCS_EXIT_INVERT_MODE
#define LCD_CMD_INVERT_ON               MIPI_DCS_ENTER_INVERT_MODE
#define LCD_CMD_NORMAL_MODE             MIPI_DCS_ENTER_NORMAL_MODE
#define LCD_CMD_PARTIAL_MODE            MIPI_DCS_ENTER_PARTIAL_MODE
#define LCD_CMD_IDLE_OFF                MIPI_DCS_EXIT_IDLE_MODE
#define LCD_CMD_IDLE_ON                 MIPI_DCS_ENTER_IDLE_MODE

/* Memory Access */
#define LCD_CMD_CASET                   MIPI_DCS_SET_COLUMN_ADDRESS
#define LCD_CMD_RASET                   MIPI_DCS_SET_PAGE_ADDRESS
#define LCD_CMD_RAMWR                   MIPI_DCS_WRITE_MEMORY_START
#define LCD_CMD_RAMRD                   MIPI_DCS_READ_MEMORY_START
#define LCD_CMD_MADCTL                  MIPI_DCS_SET_ADDRESS_MODE
#define LCD_CMD_COLMOD                  MIPI_DCS_SET_PIXEL_FORMAT

/* Tearing Effect */
#define LCD_CMD_TE_OFF                  MIPI_DCS_SET_TEAR_OFF
#define LCD_CMD_TE_ON                   MIPI_DCS_SET_TEAR_ON

/* Brightness */
#define LCD_CMD_WRITE_BRIGHTNESS        MIPI_DCS_WRITE_DISPLAY_BRIGHTNESS
#define LCD_CMD_READ_BRIGHTNESS         MIPI_DCS_READ_DISPLAY_BRIGHTNESS

#define LCD_OPCODE_WRITE_CMD            (0x02ULL)
#define LCD_OPCODE_WRITE_IMG            (0x32ULL)

/* ========================================================================
 *  MADCTL (0x36) 位定义 - Memory Access Control
 * ======================================================================== */

#define MADCTL_MY                       0x80  /* Row Address Order */
#define MADCTL_MX                       0x40  /* Column Address Order */
#define MADCTL_MV                       0x20  /* Row/Column Exchange */
#define MADCTL_ML                       0x10  /* Vertical Refresh Order */
#define MADCTL_BGR                      0x08  /* BGR Order (vs RGB) */
#define MADCTL_MH                       0x04  /* Horizontal Refresh Order */

/* ========================================================================
 *  COLMOD (0x3A) 像素格式定义
 * ======================================================================== */

#define COLMOD_RGB_12BIT                0x03  /* 12-bit/pixel */
#define COLMOD_RGB_16BIT                0x05  /* 16-bit/pixel (RGB565) */
#define COLMOD_RGB_18BIT                0x06  /* 18-bit/pixel (RGB666) */
#define COLMOD_RGB_24BIT                0x07  /* 24-bit/pixel (RGB888) */

#ifdef __cplusplus
}
#endif
