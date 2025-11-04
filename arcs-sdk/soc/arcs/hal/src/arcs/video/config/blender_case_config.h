#ifndef _BLENDER_CASE_CONFIG_H_
#define _BLENDER_CASE_CONFIG_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "Driver_Blender.h"
#include "Driver_GPDMA.h"
#include "csk_driver.h"
#include "check.h"

/*
| blender_mode | alpha_mode |           parameter          |
| ------------ | ---------- | ---------------------------- |
| 0            | 0          | error                        |
| 0            | 1          | back + color + mask + alpha  |
| 0            | 2          | back + color + alpha         |
| 1            | 0          | back + fore + fore_A + alpha |
| 1            | 1          | back + fore + mask + alpha   |
| 1            | 2          | back + fore + alpha          |

back/fore/mask is come from mem
color/alpha is come from register
fore_A is come from ARGB8888/ARGB1555/ARGB4444
*/

#define D2BLENDER_BACK_DMA_CH   gp_dma_ch0
#define D2BLENDER_FORE_DMA_CH   gp_dma_ch1
#define D2BLENDER_MASK_DMA_CH   gp_dma_ch2
#define D2BLENDER_OUT_DMA_CH    gp_dma_ch3

#define D2BLENDER_BACK_GPDMA_BURST_LEN   gpdma_burst_len_8spl
#define D2BLENDER_FORE_GPDMA_BURST_LEN   gpdma_burst_len_8spl
#define D2BLENDER_MASK_GPDMA_BURST_LEN   gpdma_burst_len_4spl
#define D2BLENDER_OUT_GPDMA_BURST_LEN    gpdma_burst_len_8spl

#define TEST_BLENDER_ALPHA_COLOR_ALL    0
#define TEST_BLENDER_CHECK_DIFF         0

#define TEST_BLENDER_IMAGE_WIDTH        64
#define TEST_BLENDER_IMAGE_HEIGHT       48

//#define BLENDER_BUF_OFFSET 0x1C
//#define BLENDER_BASK_BUF   (0x28000000 + BLENDER_BUF_OFFSET)
//#define BLENDER_FORE_BUF   (0x28040000 + BLENDER_BUF_OFFSET)
//#define BLENDER_MASK_BUF   (0x28080000 + BLENDER_BUF_OFFSET)
//#define BLENDER_OUT_BUF    (0x280C0000 + BLENDER_BUF_OFFSET)
//#define BLENDER_SWOUT_BUF  (0x28100000 + BLENDER_BUF_OFFSET)

#define BLENDER_CYCLE_ENABLE  0

#ifndef SUCCESS
#define SUCCESS     0
#endif

#ifndef FAILURE
#define FAILURE     1
#endif

static Blender_InitTypeDef blender_cfg_case0101 = {
        .blender_mode = BLENDER_MODE_FILL,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0102 = {
        .blender_mode = BLENDER_MODE_FILL,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0103 = {
        .blender_mode = BLENDER_MODE_FILL,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0201 = {
        .blender_mode = BLENDER_MODE_FILL,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0202 = {
        .blender_mode = BLENDER_MODE_FILL,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0203 = {
        .blender_mode = BLENDER_MODE_FILL,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0301 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_0,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_ARGB8888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0302 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_0,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_ARGB1555,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0303 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_0,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_ARGB4444,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0304 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_0,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB8888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0305 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_0,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB1555,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0306 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_0,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB4444,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0307 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_0,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB8888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0308 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_0,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB1555,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0309 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_0,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB4444,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0401 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_ARGB8888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0402 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_RGB888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0403 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0404 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_ARGB1555,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0405 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_ARGB4444,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0406 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_L8,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0407 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB8888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0408 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_RGB888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0409 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0410 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB1555,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0411 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB4444,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0412 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_L8,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0413 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB8888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0414 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_RGB888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0415 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0416 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB1555,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0417 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB4444,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0418 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_1,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_L8,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0501 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_ARGB8888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0502 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_RGB888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0503 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0504 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_ARGB1555,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0505 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_ARGB4444,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0506 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_L8,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0507 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB8888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0508 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_RGB888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0509 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0510 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB1555,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0511 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB4444,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0512 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB888,
        .fore_format = BLENDER_FORE_FORMAT_L8,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0513 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB8888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0514 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_RGB888,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0515 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0516 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB1555,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0517 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_ARGB4444,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

static Blender_InitTypeDef blender_cfg_case0518 = {
        .blender_mode = BLENDER_MODE_MAP,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_ARGB8888,
        .fore_format = BLENDER_FORE_FORMAT_L8,
        .img_width = TEST_BLENDER_IMAGE_WIDTH,
        .img_height = TEST_BLENDER_IMAGE_HEIGHT,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

typedef struct
{
    uint8_t index;
    uint16_t id;
    uint8_t *name;
    Blender_InitTypeDef *pcfg;
    int32_t ret;
}blender_CaseTypeDef;

blender_CaseTypeDef blender_case_tab[] = {
    { 0, 0x0101, NULL, &blender_cfg_case0101, FAILURE},
    { 1, 0x0102, NULL, &blender_cfg_case0102, FAILURE},
    { 2, 0x0103, NULL, &blender_cfg_case0103, FAILURE},
    { 3, 0x0201, NULL, &blender_cfg_case0201, FAILURE},
    { 4, 0x0202, NULL, &blender_cfg_case0202, FAILURE},
    { 5, 0x0203, NULL, &blender_cfg_case0203, FAILURE},
    { 6, 0x0301, NULL, &blender_cfg_case0301, FAILURE},
    { 7, 0x0302, NULL, &blender_cfg_case0302, FAILURE},
    { 8, 0x0303, NULL, &blender_cfg_case0303, FAILURE},
    { 9, 0x0304, NULL, &blender_cfg_case0304, FAILURE},
    {10, 0x0305, NULL, &blender_cfg_case0305, FAILURE},
    {11, 0x0306, NULL, &blender_cfg_case0306, FAILURE},
    {12, 0x0307, NULL, &blender_cfg_case0307, FAILURE},
    {13, 0x0308, NULL, &blender_cfg_case0308, FAILURE},
    {14, 0x0309, NULL, &blender_cfg_case0309, FAILURE},
    {15, 0x0401, NULL, &blender_cfg_case0401, FAILURE},
    {16, 0x0402, NULL, &blender_cfg_case0402, FAILURE},
    {17, 0x0403, NULL, &blender_cfg_case0403, FAILURE},
    {18, 0x0404, NULL, &blender_cfg_case0404, FAILURE},
    {19, 0x0405, NULL, &blender_cfg_case0405, FAILURE},
    {20, 0x0406, NULL, &blender_cfg_case0406, FAILURE},
    {21, 0x0407, NULL, &blender_cfg_case0407, FAILURE},
    {22, 0x0408, NULL, &blender_cfg_case0408, FAILURE},
    {23, 0x0409, NULL, &blender_cfg_case0409, FAILURE},
    {24, 0x0410, NULL, &blender_cfg_case0410, FAILURE},
    {25, 0x0411, NULL, &blender_cfg_case0411, FAILURE},
    {26, 0x0412, NULL, &blender_cfg_case0412, FAILURE},
    {27, 0x0413, NULL, &blender_cfg_case0413, FAILURE},
    {28, 0x0414, NULL, &blender_cfg_case0414, FAILURE},
    {29, 0x0415, NULL, &blender_cfg_case0415, FAILURE},
    {30, 0x0416, NULL, &blender_cfg_case0416, FAILURE},
    {31, 0x0417, NULL, &blender_cfg_case0417, FAILURE},
    {32, 0x0418, NULL, &blender_cfg_case0418, FAILURE},
    {33, 0x0501, NULL, &blender_cfg_case0501, FAILURE},
    {34, 0x0502, NULL, &blender_cfg_case0502, FAILURE},
    {35, 0x0503, NULL, &blender_cfg_case0503, FAILURE},
    {36, 0x0504, NULL, &blender_cfg_case0504, FAILURE},
    {37, 0x0505, NULL, &blender_cfg_case0505, FAILURE},
    {38, 0x0506, NULL, &blender_cfg_case0506, FAILURE},
    {39, 0x0507, NULL, &blender_cfg_case0507, FAILURE},
    {40, 0x0508, NULL, &blender_cfg_case0508, FAILURE},
    {41, 0x0509, NULL, &blender_cfg_case0509, FAILURE},
    {42, 0x0510, NULL, &blender_cfg_case0510, FAILURE},
    {43, 0x0511, NULL, &blender_cfg_case0511, FAILURE},
    {44, 0x0512, NULL, &blender_cfg_case0512, FAILURE},
    {45, 0x0513, NULL, &blender_cfg_case0513, FAILURE},
    {46, 0x0514, NULL, &blender_cfg_case0514, FAILURE},
    {47, 0x0515, NULL, &blender_cfg_case0515, FAILURE},
    {48, 0x0516, NULL, &blender_cfg_case0516, FAILURE},
    {49, 0x0517, NULL, &blender_cfg_case0517, FAILURE},
    {50, 0x0518, NULL, &blender_cfg_case0518, FAILURE},
};

#endif

