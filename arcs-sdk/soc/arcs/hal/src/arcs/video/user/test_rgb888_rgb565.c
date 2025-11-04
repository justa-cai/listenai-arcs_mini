#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "chip.h"
#include "log_print.h"
#include "systick.h"
#include "ClockManager.h"
#include "PSRAMManager.h"
#include "Driver_GPDMA.h"
#include "Driver_DMA2D.h"
#include "Driver_Blender.h"

#include "test_case.h"
#include "csk_driver.h"

#define RGB565_BLENDER_BACK_DMA_CH   gp_dma_ch2
#define RGB888_BLENDER_FORE_DMA_CH   gp_dma_ch3
#define RGB565_BLENDER_OUT_DMA_CH    gp_dma_ch4

#define RGB565_PIXEL_BYTE   2
#define RGB888_PIXEL_BYTE   3

static int32_t blender_rgb888_to_rgb565(void *rgb888, void *rgb565, uint16_t img_width, uint16_t img_height);

static int32_t test_rgb888_to_rgb565(void);

void test_rgb888_rgb565(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    CHECK_FUNC_EXIT(test_rgb888_to_rgb565(), error);

    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return;

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return;
}


static int32_t test_rgb888_to_rgb565(void)
{
    int32_t ret = FAILURE;
    uint8_t *rgb888_buf = NULL;
    uint8_t *rgb565_buf = NULL;
    uint32_t rgb888_size_byte = 0;
    uint32_t rgb565_size_byte = 0;
    uint16_t img_width = 320;
    uint16_t img_height = 240;

    rgb888_size_byte = img_width * img_height * RGB888_PIXEL_BYTE;
    rgb565_size_byte = img_width * img_height * RGB565_PIXEL_BYTE;
    VIDEO_LOG("[%s:%d] rgb888_size_byte=0x%x", __func__, __LINE__, rgb888_size_byte);
    VIDEO_LOG("[%s:%d] rgb565_size_byte=0x%x", __func__, __LINE__, rgb565_size_byte);

    /* malloc */
    rgb888_buf = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(rgb888_buf, error);
    VIDEO_LOG("rgb888_buf=0x%08x size=0x%x byte", rgb888_buf, rgb888_size_byte);

    rgb565_buf = tiny_malloc(rgb565_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(rgb565_buf, error);
    VIDEO_LOG("rgb565_buf=0x%08x size=0x%x byte", rgb565_buf, rgb565_size_byte);

    rgb888_colorbar_create(rgb888_buf, img_width, img_height, 40);
    memset(rgb565_buf, 0, rgb565_size_byte);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_BLENDER_CLK = 0x1; // blender clk enable

    ret = GPDMA_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = blender_rgb888_to_rgb565(rgb888_buf, rgb565_buf, img_width, img_height);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    //while(1);

    ret = SUCCESS;

error:
    tiny_free(rgb888_buf);
    tiny_free(rgb565_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static volatile uint32_t blender_dma_finish_flag = 0;

static void d2blender_out_dma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    blender_dma_finish_flag++;
}

static int32_t blender_rgb888_to_rgb565(void *rgb888, void *rgb565, uint16_t img_width, uint16_t img_height)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t back_size_byte = 0;
    uint32_t fore_size_byte = 0;
    uint32_t out_size_byte = 0;
    uint16_t rgb565_value = 0xFFFF;

    void *blender_dev = Blender0();

    static Blender_InitTypeDef blender_cfg = {
            .blender_mode = BLENDER_MODE_MAP,
            .alpha_mode = BLENDER_ALPHA_MODE_2,
            .back_format = BLENDER_BACK_FORMAT_RGB565,
            .fore_format = BLENDER_FORE_FORMAT_RGB888,
            .img_width = 0,
            .img_height = 0,
            .color = 0x808080,
            .alpha = 0xFF,
            .burst_thd = 1,
    };

    static csk_gpdma_init_t d2back_input = {
            .dma_ch = RGB565_BLENDER_BACK_DMA_CH,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2p,
            .src_inc_mode = inc_mode_fix,
            .dst_inc_mode = inc_mode_fix,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = d2back_hs_num3,
    };

    static csk_gpdma_init_t d2fore_input = {
            .dma_ch = RGB888_BLENDER_FORE_DMA_CH,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2p,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_fix,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = d2fore_hs_num4,
    };

    static csk_gpdma_init_t d2out_output = {
            .dma_ch = RGB565_BLENDER_OUT_DMA_CH,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_p2m,
            .src_inc_mode = inc_mode_fix,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = d2out_hs_num1,
    };

    blender_cfg.img_width = img_width;
    blender_cfg.img_height = img_height;

    back_size_byte = img_width * img_height * RGB565_PIXEL_BYTE;
    fore_size_byte = img_width * img_height * RGB888_PIXEL_BYTE;
    out_size_byte = back_size_byte;

    /* blender init */
    ret = Blender_Initialize(blender_dev, &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    /* GPDMA init */
    ret = GPDMA_Config(&d2back_input, NULL, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&d2fore_input, NULL, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&d2out_output, d2blender_out_dma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* GPDMA start */
    ret = GPDMA_Start_Normal(d2back_input.dma_ch, &rgb565_value, (void*)D2BACK_BUF, back_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Start_Normal(d2fore_input.dma_ch, rgb888, (void*)D2FORE_BUF, fore_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Start_Normal(d2out_output.dma_ch, (void*)D2OUT_BUF, rgb565, out_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    blender_dma_finish_flag = 0;

    /* blender start */
    Blender_Start(blender_dev);

    timeout = 1000000;  // wait blender done, timeout=1000ms
    while(!blender_dma_finish_flag)
    {
        DELAY_US(1);
        if(timeout-- == 0)
        {
            VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
            blender_reg_dump();
            ret = FAILURE;
            goto error;
        }
    }

    Blender_Stop(blender_dev);

    ret = SUCCESS;

error:
    return ret;
}

