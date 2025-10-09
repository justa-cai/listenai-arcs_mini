#ifndef _DVP_CASE_CONFIG_H_
#define _DVP_CASE_CONFIG_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "Driver_DVP.h"
#include "Driver_GPDMA.h"
#include "csk_driver.h"
#include "camera.h"

#define TEST_DVP_GPDMA_CH           gp_dma_ch1
#define TEST_DVP_GPDMA_AUTO_MODE    1
#define TEST_DVP_MCLK_HZ            24000000     // Hz

static DVP_InitTypeDef dvp_cfg_case0101 = {
        .FrameWidth = 1280,
        .FrameHeight = 720,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,  // DVP_DATA_ALIGN_LEFT:bit11~4  DVP_DATA_ALIGN_RIGHT:bit7~0
};
static camera_config_t camera_cfg_case0101 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_HD,
    .colorbar = 0,
};

static DVP_InitTypeDef dvp_cfg_case0102 = {
        .FrameWidth = 640,
        .FrameHeight = 480,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
};
static camera_config_t camera_cfg_case0102 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_VGA,
    .colorbar = 0,
};

static DVP_InitTypeDef dvp_cfg_case0103 = {
        .FrameWidth = 320,
        .FrameHeight = 240,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
};
static camera_config_t camera_cfg_case0103 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};

static DVP_InitTypeDef dvp_cfg_case0104 = {     // YUV422 320x240
        .FrameWidth = 640,
        .FrameHeight = 240,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_LUMINA_8BIT,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
};
static camera_config_t camera_cfg_case0104 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};

static DVP_InitTypeDef dvp_cfg_case0105 = {     // RGB565 320x240
        .FrameWidth = 640,
        .FrameHeight = 240,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_LUMINA_8BIT,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
};
static camera_config_t camera_cfg_case0105 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};


static DVP_InitTypeDef dvp_cfg_case0201 = {
        .FrameWidth = 160,
        .FrameHeight = 120,
        .PixelOffset = 80,      // byte
        .LineOffset = 60,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
};
static camera_config_t camera_cfg_case0201 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};

static DVP_InitTypeDef dvp_cfg_case0202 = {
        .FrameWidth = 320,
        .FrameHeight = 120,
        .PixelOffset = 160,
        .LineOffset = 60,
        .InputFormat = DVP_INPUT_FORM_LUMINA_8BIT,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
};
static camera_config_t camera_cfg_case0202 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};


static DVP_InitTypeDef dvp_cfg_case0301 = {
        .FrameWidth = 320,
        .FrameHeight = 240,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
        .PCKPolarity = DVP_POL_FALLING,          // DVP_POL_RISING / DVP_POL_FALLING
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,      // DVP_DATA_ALIGN_LEFT / DVP_DATA_ALIGN_RIGHT
};
static camera_config_t camera_cfg_case0301 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};

static DVP_InitTypeDef dvp_cfg_case0302 = {
        .FrameWidth = 320,
        .FrameHeight = 240,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_FALLING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
};
static camera_config_t camera_cfg_case0302 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};

static DVP_InitTypeDef dvp_cfg_case0303 = {
        .FrameWidth = 320,
        .FrameHeight = 240,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_FALLING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
};
static camera_config_t camera_cfg_case0303 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};

static DVP_InitTypeDef dvp_cfg_case0304 = {
        .FrameWidth = 320,
        .FrameHeight = 240,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
};
static camera_config_t camera_cfg_case0304 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};

static DVP_InitTypeDef dvp_cfg_case0501 = {
        .FrameWidth = 960,
        .FrameHeight = 80,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
};
static camera_config_t camera_cfg_case0501 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};

static DVP_InitTypeDef dvp_cfg_case0502 = {
        .FrameWidth = 80,
        .FrameHeight = 960,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
};
static camera_config_t camera_cfg_case0502 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};

typedef struct
{
    uint8_t index;
    uint16_t id;
    uint8_t *name;
    DVP_InitTypeDef *pdvp_cfg;
    camera_config_t *pcamera_cfg;
    int32_t ret;
}dvp_CaseTypeDef;

dvp_CaseTypeDef dvp_case_tab[] = {
    { 0, 0x0101, NULL, &dvp_cfg_case0101, &camera_cfg_case0101, FAILURE},
    { 1, 0x0102, NULL, &dvp_cfg_case0102, &camera_cfg_case0102, FAILURE},
    { 2, 0x0103, NULL, &dvp_cfg_case0103, &camera_cfg_case0103, FAILURE},
    { 3, 0x0104, NULL, &dvp_cfg_case0104, &camera_cfg_case0104, FAILURE},
    { 4, 0x0105, NULL, &dvp_cfg_case0105, &camera_cfg_case0105, FAILURE},
    //{ 5, 0x0106, NULL, &dvp_cfg_case0106, &camera_cfg_case0106, FAILURE},
    //{ 6, 0x0107, NULL, &dvp_cfg_case0107, &camera_cfg_case0107, FAILURE},
    //{ 7, 0x0108, NULL, &dvp_cfg_case0108, &camera_cfg_case0108, FAILURE},
    //{ 8, 0x0109, NULL, &dvp_cfg_case0109, &camera_cfg_case0109, FAILURE},
    //{ 9, 0x0201, NULL, &dvp_cfg_case0201, &camera_cfg_case0201, FAILURE},
    //{10, 0x0202, NULL, &dvp_cfg_case0202, &camera_cfg_case0202, FAILURE},
    //{11, 0x0203, NULL, &dvp_cfg_case0203, &camera_cfg_case0203, FAILURE},
    //{12, 0x0301, NULL, &dvp_cfg_case0301, &camera_cfg_case0301, FAILURE},
};

#endif

