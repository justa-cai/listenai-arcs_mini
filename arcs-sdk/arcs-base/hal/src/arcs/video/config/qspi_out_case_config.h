#ifndef _QSPI_OUT_CASE_CONFIG_H_
#define _QSPI_OUT_CASE_CONFIG_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "Driver_QSPI_LCD.h"
#include "Driver_GPDMA.h"
#include "csk_driver.h"
#include "csk_qspi_lcd.h"

#define TEST_QSPI_OUT_GPDMA_CH          gp_dma_ch1
#define TEST_QSPI_OUT_CLK_HZ            24000000

// 412x412 -> 320x240  Center=(206,206)  start=(206-160,206-120)=(46,86)
#define TEST_QSPI_OUT_IMAGE_START_X       48
#define TEST_QSPI_OUT_IMAGE_START_Y       86
#define TEST_QSPI_OUT_IMAGE_SIZE_W       320
#define TEST_QSPI_OUT_IMAGE_SIZE_H       240

#define TEST_QSPI_OUT_LCD_WIDTH         172
#define TEST_QSPI_OUT_LCD_HEIGHT        640

static qspi_lcd_config_t qspi_out_cfg_case0101 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_PIO,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 1,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB888,
};

static qspi_lcd_config_t qspi_out_cfg_case0102 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_PIO,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 2,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB888,
};

static qspi_lcd_config_t qspi_out_cfg_case0103 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_PIO,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 4,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB888,
};

static qspi_lcd_config_t qspi_out_cfg_case0201 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_DMA,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 1,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB888,
};

static qspi_lcd_config_t qspi_out_cfg_case0202 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_DMA,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 2,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB888,
};

static qspi_lcd_config_t qspi_out_cfg_case0203 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_DMA,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 4,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB888,
};

static qspi_lcd_config_t qspi_out_cfg_case0301 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_PIO,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 1,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB565,
};

static qspi_lcd_config_t qspi_out_cfg_case0302 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_PIO,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 2,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB565,
};

static qspi_lcd_config_t qspi_out_cfg_case0303 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_PIO,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 4,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB565,
};

static qspi_lcd_config_t qspi_out_cfg_case0401 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_DMA,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 1,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB565,
};

static qspi_lcd_config_t qspi_out_cfg_case0402 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_DMA,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 2,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB565,
};

static qspi_lcd_config_t qspi_out_cfg_case0403 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_DMA,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 4,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB565,
};

static qspi_lcd_config_t qspi_out_cfg_case0501 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_PIO,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 1,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB565,
};

static qspi_lcd_config_t qspi_out_cfg_case0502 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_DMA,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 1,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB565,
};

static qspi_lcd_config_t qspi_out_cfg_case0601 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,     //
        .txio = QSPI_LCD_TXIO_DMA,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 4,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB888,
};

static qspi_lcd_config_t qspi_out_cfg_case0602 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_DMA,
        .cp = QSPI_LCD_CPOL1_CPOH1,         // CPOL0_CPOH0 / CPOL0_CPOH1 / CPOL1_CPOH0 / CPOL1_CPOH1
        .is_msb = true,
        .lane_num = 4,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB888,
};

static qspi_lcd_config_t qspi_out_cfg_case0603 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_DMA,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,                     // true / false
        .lane_num = 4,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB888,
};

static qspi_lcd_config_t qspi_out_cfg_case0604 = {
        .clk_hz = TEST_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_DMA,
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 4,
        .width = TEST_QSPI_OUT_LCD_WIDTH,
        .height = TEST_QSPI_OUT_LCD_HEIGHT,
        .format = QSPI_LCD_FORMAT_RGB888,
};


typedef struct
{
    uint8_t index;
    uint16_t id;
    uint8_t *name;
    qspi_lcd_config_t *pcfg;
    int32_t ret;
}qspi_out_CaseTypeDef;

qspi_out_CaseTypeDef qspi_out_case_tab[] = {
    { 0, 0x0101, NULL, &qspi_out_cfg_case0101, FAILURE},
    { 1, 0x0102, NULL, &qspi_out_cfg_case0102, FAILURE},
    { 2, 0x0103, NULL, &qspi_out_cfg_case0103, FAILURE},
    { 3, 0x0201, NULL, &qspi_out_cfg_case0201, FAILURE},
    { 4, 0x0202, NULL, &qspi_out_cfg_case0202, FAILURE},
    { 5, 0x0203, NULL, &qspi_out_cfg_case0203, FAILURE},
    { 6, 0x0301, NULL, &qspi_out_cfg_case0301, FAILURE},
    { 7, 0x0302, NULL, &qspi_out_cfg_case0302, FAILURE},
    { 8, 0x0303, NULL, &qspi_out_cfg_case0303, FAILURE},
    { 9, 0x0401, NULL, &qspi_out_cfg_case0401, FAILURE},
    {10, 0x0402, NULL, &qspi_out_cfg_case0402, FAILURE},
    {11, 0x0403, NULL, &qspi_out_cfg_case0403, FAILURE},
    {12, 0x0501, NULL, &qspi_out_cfg_case0501, FAILURE},
    {13, 0x0502, NULL, &qspi_out_cfg_case0502, FAILURE},
    {14, 0x0601, NULL, &qspi_out_cfg_case0601, FAILURE},
    {15, 0x0602, NULL, &qspi_out_cfg_case0602, FAILURE},
    {16, 0x0603, NULL, &qspi_out_cfg_case0603, FAILURE},
    {17, 0x0604, NULL, &qspi_out_cfg_case0604, FAILURE},
};

#endif

