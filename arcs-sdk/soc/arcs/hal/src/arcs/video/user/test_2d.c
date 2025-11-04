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
#include "Driver_DVP.h"
#include "Driver_QSPI_LCD.h"
#include "Driver_Blender.h"

#include "test_case.h"
#include "csk_driver.h"
#include "camera.h"
#include "lcd.h"
#include "spd2010.h"

#define TEST_2D_GPDMA_CH                dma_2d_ch6
#define TEST_2D_ROTATE1_GPDMA_CH        dma_2d_ch8
#define TEST_2D_ROTATE2_GPDMA_CH        dma_2d_ch9

#define TEST_2D_IMAGE_WIDTH             320
#define TEST_2D_IMAGE_HEIGHT            240

#define RGB565_PIXEL_BYTE   2
#define RGB888_PIXEL_BYTE   3

typedef enum _csk_dma2d_format {
    DMA2D_FORMAT_RGB565 = 0,
    DMA2D_FORMAT_RGB888,
    DMA2D_FORMAT_BUTT,
} csk_dma2d_format_t;

static int32_t test_2d_yuv422_to_rgb888(void);
static int32_t test_2d_yuv422_rotate90(void);
static int32_t test_2d_argb8888_rotate90(void);
static int32_t test_2d_rgb888_copy0(void);    // src step, dts not step
static int32_t test_2d_rgb888_copy1(void);    // src not step, dts step
static int32_t test_2d_rgb888_copy2(void);    // src step, dts step
static int32_t test_2d_rgb888_copy3(void);    // src not step, dts step
static int32_t test_2d_rgb888_copy4(void);    // copy0 -> blender -> copy1

static int32_t test_2d_rgb565_copy0(void);      // src step, dts not step
static int32_t test_2d_rgb565_copy00(void);     // src step, dts not step
static int32_t test_2d_rgb565_copy1(void);      // src not step, dts step
static int32_t test_2d_rgb565_copy11(void);     // src not step, dts step
static int32_t test_2d_rgb565_copy22(void);     // src step, dts step

static int32_t dma2d_argb8888_rotate90(csk_dma2d_ch_t dma_ch, void *src_buf, void *dts_buf, uint16_t img_width, uint16_t img_height);
static int32_t test_2d_argb8888_rotate90_00(void);
static int32_t test_2d_rgb565_rotate90(void);    // fail


static int32_t test_2d_src_step(csk_dma2d_ch_t dma_ch, csk_dma2d_format_t format, void *src_buf, void *dts_buf, uint16_t src_width, uint16_t dts_width, uint16_t dts_height);
static int32_t test_2d_dts_step(csk_dma2d_ch_t dma_ch, csk_dma2d_format_t format, void *src_buf, void *dts_buf, uint16_t dts_width, uint16_t src_width, uint16_t src_height);

static int32_t lv_gpu_stm32_dma2d_copy(void *buf, uint16_t buf_w, void *map, uint16_t map_w, uint16_t copy_w, uint16_t copy_h);
static int32_t lv_gpu_stm32_dma2d_fill(void *buf, uint16_t buf_w, uint32_t color, uint16_t fill_w, uint16_t fill_h);
static int32_t lv_gpu_stm32_dma2d_blend(void *buf, uint16_t buf_w, void *map, uint8_t opa, uint16_t map_w, uint16_t copy_w, uint16_t copy_h);

static int32_t test_2d_rgb565_copy(void);    // src step, dts step   lv_gpu_stm32_dma2d_copy
static int32_t test_2d_rgb565_fill(void);
static int32_t test_2d_rgb565_blend(void);


void test_2d(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    test_2d_yuv422_to_rgb888();
    //test_2d_yuv422_rotate90();
    //test_2d_argb8888_rotate90();
    //test_2d_rgb888_copy0();
    //test_2d_rgb888_copy1();
    //test_2d_rgb888_copy2();
    //test_2d_rgb888_copy3();
    //test_2d_rgb888_copy4();

    //test_2d_rgb565_copy0();
    //test_2d_rgb565_copy00();
    //test_2d_rgb565_copy1();
    //test_2d_rgb565_copy11();
    //test_2d_rgb565_copy22();

    //test_2d_rgb565_copy();
    //test_2d_rgb565_fill();
    //test_2d_rgb565_blend();

    //test_2d_argb8888_rotate90_00();
    //test_2d_rgb565_rotate90();  // fail

    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return;

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return;
}

static csk_dma2d_init_t gpdma2d_cfg = {
    .dma_ch = TEST_2D_GPDMA_CH,
    .burst_len = dma2d_burst_len_1spl,
    .src_mode = address_mode_normal,
    .dst_mode = address_mode_normal,
    .tfr_mode = tfr_mode_m2m,
    .sample_unit = dma2d_sample_unit_word,
    .src_inc_mode = inc_mode_increase,
    .dst_inc_mode = inc_mode_increase,
    .prio_lvl = prio_mode_vhigh,
    .rd_done_ack = read_done_ack_enable,
    .handshake = hs_none,
};

static csk_dma_2d_image_cfg_t gpdma2d_img_cfg = {
    .img_input_format = csk_image_format_yuv422,
    .img_width = TEST_2D_IMAGE_WIDTH,
    .img_height = TEST_2D_IMAGE_HEIGHT,
    .img_output_fromat_transfer = csk_image_format_transfer_yuv422_xrgb,
    .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    .img_rgb888_format = csk_image_rgb888_format,
};


static volatile uint32_t test_gpdma_2d_finish_cnt = 0;

static void gpdma_2d_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_gpdma_2d_finish_cnt++;
}

static int32_t test_2d_yuv422_to_rgb888(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    bool dma_en = false;
    uint8_t *yuv422_buf = NULL;
    uint8_t *rgb888_buf = NULL;
    uint8_t *rgb565_buf = NULL;
    uint32_t yuv422_size_byte = TEST_2D_IMAGE_WIDTH * TEST_2D_IMAGE_HEIGHT * 2;
    uint32_t rgb888_size_byte = TEST_2D_IMAGE_WIDTH * TEST_2D_IMAGE_HEIGHT * 3;
    uint32_t image_cnt = 0;
    uint32_t gpdma2d_cnt = 0;
    uint32_t i = 0;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    yuv422_buf = tiny_malloc(yuv422_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(yuv422_buf, error1);
    VIDEO_LOG("yuv422_buf=0x%08x size=0x%x byte", yuv422_buf, yuv422_size_byte);

    rgb888_buf = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(rgb888_buf, error1);
    VIDEO_LOG("rgb888_buf=0x%08x size=0x%x byte", rgb888_buf, rgb888_size_byte);

    rgb565_buf = tiny_malloc(yuv422_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(rgb565_buf, error1);
    VIDEO_LOG("rgb565_buf=0x%08x size=0x%x byte", rgb565_buf, yuv422_size_byte);

    /* gpdma init */
    ret = GPDMA_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&gpdma2d_cfg, gpdma_2d_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(gpdma2d_cfg.dma_ch, &gpdma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    rgb565_colorbar_create((uint16_t *)rgb565_buf, TEST_2D_IMAGE_WIDTH, TEST_2D_IMAGE_HEIGHT, 20);
    //rgb565_grid_create((uint16_t *)rgb565_buf, TEST_2D_IMAGE_WIDTH, TEST_2D_IMAGE_HEIGHT, 20);
    rgb565_to_yuv422yuyv((uint16_t *)rgb565_buf, yuv422_buf, TEST_2D_IMAGE_WIDTH * TEST_2D_IMAGE_HEIGHT);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1)
    {
        /* FIFO clear */
        mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_cfg.dma_ch));

        gpdma2d_cnt = test_gpdma_2d_finish_cnt;
        ret = DMA2D_Start_Normal(gpdma2d_cfg.dma_ch, yuv422_buf, rgb888_buf, yuv422_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
        timeout = 3000000;  // wait blender done, timeout=1000ms
        CHECK_EQ_TIMEOUT_EXIT(gpdma2d_cnt, test_gpdma_2d_finish_cnt, timeout, error2);
        gpdma2d_cnt = test_gpdma_2d_finish_cnt;
        VIDEO_LOG("gpdma2d_cnt=%d", gpdma2d_cnt);
        VIDEO_LOG("rgb888_buf=0x%08x size=0x%x byte", rgb888_buf, rgb888_size_byte);

        /* FIFO clear */
        mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_cfg.dma_ch));

        ret = DMA2D_Image_Config_Extend(gpdma2d_cfg.dma_ch, &gpdma2d_img_cfg);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

        DELAY_MS(10);
    }

    ret = SUCCESS;

error2:
error1:
    tiny_free(yuv422_buf);
    tiny_free(rgb888_buf);
    tiny_free(rgb565_buf);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static csk_dma_2d_rotate_cfg_t gpdam2d_rotate_para_1= {
    .dma2d_init = {
        .dma_ch = TEST_2D_ROTATE1_GPDMA_CH,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    },

    .dma2d_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = TEST_2D_IMAGE_WIDTH,
        .img_height = TEST_2D_IMAGE_HEIGHT,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_cw_90,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
        .image_yuv422_rotate_mode = csk_image_yuv422_clockwise_rotate,
    }
};

static csk_dma_2d_rotate_cfg_t gpdam2d_rotate_para_2= {
    .dma2d_init = {
        .dma_ch = TEST_2D_ROTATE2_GPDMA_CH,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    },

    .dma2d_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = TEST_2D_IMAGE_WIDTH,
        .img_height = TEST_2D_IMAGE_HEIGHT,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    }
};


static volatile uint32_t test_gpdma_2d_rotate1_finish_cnt = 0;
static volatile uint32_t test_gpdma_2d_rotate2_finish_cnt = 0;

static void gpdma_2d_rotate1_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_gpdma_2d_rotate1_finish_cnt++;
}

static void gpdma_2d_rotate2_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_gpdma_2d_rotate2_finish_cnt++;
}

/* camera 320x240 YUV422 -> DVP auto buf PSRAM -> rotate 90 -> YUV422 to Y8 -> YUV422 to RGB888 -> crop + zoom -> blender mask+color -> qspi_out */
static int32_t test_2d_yuv422_rotate90(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint8_t *yuv422_buf = NULL;
    uint8_t *yuv422_rotate_buf = NULL;
    uint8_t *yuv422_8line_buf1 = NULL;
    uint8_t *yuv422_8line_buf2 = NULL;
    uint32_t yuv422_size_byte = TEST_2D_IMAGE_WIDTH * TEST_2D_IMAGE_HEIGHT * 2;
    uint32_t yuv422_8line_size_byte = TEST_2D_IMAGE_WIDTH * 8 * 2;
    uint32_t rotate1_cnt = 0;
    uint32_t rotate2_cnt = 0;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    yuv422_buf = tiny_malloc(yuv422_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(yuv422_buf, error1);
    VIDEO_LOG("yuv422_buf=0x%08x size=0x%x byte", yuv422_buf, yuv422_size_byte);

    yuv422_rotate_buf = tiny_malloc(yuv422_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(yuv422_rotate_buf, error1);
    VIDEO_LOG("yuv422_rotate_buf=0x%08x size=0x%x byte", yuv422_rotate_buf, yuv422_size_byte);

    yuv422_8line_buf1 = tiny_malloc(yuv422_8line_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(yuv422_8line_buf1, error1);
    VIDEO_LOG("yuv422_8line_buf1=0x%08x size=0x%x byte", yuv422_8line_buf1, yuv422_8line_size_byte);

    yuv422_8line_buf2 = tiny_malloc(yuv422_8line_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(yuv422_8line_buf2, error1);
    VIDEO_LOG("yuv422_8line_buf2=0x%08x size=0x%x byte", yuv422_8line_buf2, yuv422_8line_size_byte);

    /* gpdma init */
    ret = GPDMA_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Rotate_Config(&gpdam2d_rotate_para_1, &gpdam2d_rotate_para_2, gpdma_2d_rotate1_callback, gpdma_2d_rotate2_callback, NULL, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Rotate_Config_Extend(&gpdam2d_rotate_para_1, &gpdam2d_rotate_para_2);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    rgb565_colorbar_create((uint16_t *)yuv422_buf, TEST_2D_IMAGE_WIDTH, TEST_2D_IMAGE_HEIGHT, 20);
    //rgb565_grid_create((uint16_t *)rgb565_buf, TEST_2D_IMAGE_WIDTH, TEST_2D_IMAGE_HEIGHT, 20);
    rgb565_to_yuv422yuyv((uint16_t *)yuv422_buf, yuv422_buf, TEST_2D_IMAGE_WIDTH * TEST_2D_IMAGE_HEIGHT);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    //while(1);

    while(1)
    {
        /* FIFO clear */
        mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdam2d_rotate_para_1.dma2d_init.dma_ch));
        mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdam2d_rotate_para_2.dma2d_init.dma_ch));

        /* 2D: rotate 90 */
        rotate1_cnt = test_gpdma_2d_rotate1_finish_cnt;
        rotate2_cnt = test_gpdma_2d_rotate2_finish_cnt;
 //       ret = DMA2D_Start_Rotate(&gpdam2d_rotate_para_1, &gpdam2d_rotate_para_2, yuv422_buf, yuv422_8line_buf1, yuv422_8line_buf2, yuv422_rotate_buf, yuv422_size_byte / sizeof(uint32_t));
        ret = DMA2D_Start_Rotate(&gpdam2d_rotate_para_1, &gpdam2d_rotate_para_2, yuv422_buf, yuv422_rotate_buf, yuv422_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
        timeout = 3000000;  // wait blender done, timeout=1000ms
        CHECK_EQ_TIMEOUT_EXIT(rotate1_cnt, test_gpdma_2d_rotate1_finish_cnt, timeout, error2);
        timeout = 3000000;
        CHECK_EQ_TIMEOUT_EXIT(rotate2_cnt, test_gpdma_2d_rotate2_finish_cnt, timeout, error2);
        rotate1_cnt = test_gpdma_2d_rotate1_finish_cnt;
        rotate2_cnt = test_gpdma_2d_rotate2_finish_cnt;
        VIDEO_LOG("rotate1_cnt=%d", rotate1_cnt);
        VIDEO_LOG("rotate2_cnt=%d", rotate2_cnt);

        VIDEO_LOG("[%s:%d]", __func__, __LINE__);
        while(1);

        /* FIFO clear */
        mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdam2d_rotate_para_1.dma2d_init.dma_ch));
        mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdam2d_rotate_para_2.dma2d_init.dma_ch));

        /* 2D: rotate 90 config */
        ret = DMA2D_Image_Rotate_Config_Extend(&gpdam2d_rotate_para_1, &gpdam2d_rotate_para_2);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

        DELAY_MS(1000);
    }

    ret = SUCCESS;

error2:
error1:
    tiny_free(yuv422_buf);
    tiny_free(yuv422_rotate_buf);
    tiny_free(yuv422_8line_buf1);
    tiny_free(yuv422_8line_buf2);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_2d_argb8888_rotate90(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint8_t *argb8888_buf = NULL;
    uint8_t *argb8888_rotate_buf = NULL;
    uint32_t argb8888_size_byte = TEST_2D_IMAGE_WIDTH * TEST_2D_IMAGE_HEIGHT * 4;
    uint32_t buf_offset = 0;
    uint32_t rotate1_cnt = 0;

    csk_dma_2d_rotate_cfg_t gpdam2d_argb8888_rotate_para= {
        .dma2d_init = {
            .dma_ch = dma_2d_ch9,
            .burst_len = dma2d_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = dma2d_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .rd_done_ack = read_done_ack_enable,
            .handshake = hs_none,
        },

        .dma2d_img_cfg = {
            .img_input_format = csk_image_format_argb,
            .img_width = TEST_2D_IMAGE_WIDTH,
            .img_height = TEST_2D_IMAGE_HEIGHT,
            .img_output_fromat_transfer = csk_image_format_transfer_yuv422_cw_90,
            .img_rgb888_format = csk_image_rgb888_format,
            .image_yuv422_rotate_mode = csk_image_yuv422_clockwise_rotate,
        }
    };

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    argb8888_buf = tiny_malloc(argb8888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(argb8888_buf, error1);
    VIDEO_LOG("argb8888_buf=0x%08x size=0x%x byte", argb8888_buf, argb8888_size_byte);

    argb8888_rotate_buf = tiny_malloc(argb8888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(argb8888_rotate_buf, error1);
    VIDEO_LOG("argb8888_rotate_buf=0x%08x size=0x%x byte", argb8888_rotate_buf, argb8888_size_byte);

    argb8888_colorbar_create(argb8888_buf, TEST_2D_IMAGE_WIDTH, TEST_2D_IMAGE_HEIGHT, 20);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);


    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&gpdam2d_argb8888_rotate_para.dma2d_init, gpdma_2d_rotate1_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(gpdam2d_argb8888_rotate_para.dma2d_init.dma_ch, &gpdam2d_argb8888_rotate_para.dma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << gpdam2d_argb8888_rotate_para.dma2d_init.dma_ch);
    rotate1_cnt = test_gpdma_2d_rotate1_finish_cnt;

    VIDEO_LOG("in width %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9);
    VIDEO_LOG("in height %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9);
    VIDEO_LOG("out width %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9);
    VIDEO_LOG("out height %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9);
    VIDEO_LOG("step_s %d", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9);
    VIDEO_LOG("step_l0 %d", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9);

    IP_DMA2D->REG_DMA_BLOCK_LEN_CH9.bit.CFG_BLOCK_LEN_CH9 = gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_width * gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_height;
    IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH9.bit.CFG_IMAGE_OUT_BLOCK_LEN_CH9 = gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_width * gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_height;
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9 = gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_width;
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9 = gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_height;
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9 = gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_width;
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9 = gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_height;
    IP_DMA2D->REG_DMA_IMAGE_FORMAT.bit.CFG_YUV_INPUT_FORMAT_CH9 = 3; // 0:422 1:420 2:444 3:argb8888

    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_RGB_BYPASS = (1<<3);   // ch9
    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_YUV422_BYPASS = (1<<3); // ch9
    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS1.bit.CFG_YUV_UNPACK_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_ENC_OUT2D_BYPASS.bit.CFG_ENC_OUT2D_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_DEC_OUT2D_BYPASS.bit.CFG_DEC_OUT2D_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_DEC_IN2D_BYPASS.bit.CFG_DEC_IN2D_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_ZOOM_MODE.bit.CFG_ZOOM_OUT_MODE_CH9 = 0;
    IP_DMA2D->REG_DMA_CH9_CTRL.bit.CFG_READ_DONE_ACK_EN_CH9 = 1;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_BYPASS_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_SW_CTRL_CH9 = 1;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_WNUM_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_HNUM_CH9 = gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_height;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = 0 - gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_width * 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9.bit.CFG_D2_ADDR_STEP_L1_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9.bit.CFG_D2_ADDR_STEP_L2_CH9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 2;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM1_CH9 = gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_width;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM2_CH9 = 1;

    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_ROTA_MODE_SEL_9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_YUV2Y_EN_9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_RGB2Y_EN_9 = 0;

    buf_offset = (gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_height - 1) * gpdam2d_argb8888_rotate_para.dma2d_img_cfg.img_width * 4;
    ret = DMA2D_Start_Normal(gpdam2d_argb8888_rotate_para.dma2d_init.dma_ch, argb8888_buf + buf_offset, argb8888_rotate_buf, argb8888_size_byte);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(rotate1_cnt, test_gpdma_2d_rotate1_finish_cnt, timeout, error2);
    rotate1_cnt = test_gpdma_2d_rotate1_finish_cnt;
    VIDEO_LOG("rotate1_cnt=%d", rotate1_cnt);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    while(1);

    ret = SUCCESS;

error2:
error1:
    tiny_free(argb8888_buf);
    tiny_free(argb8888_rotate_buf);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* width or height < 8192 */
static int32_t dma2d_argb8888_rotate90(csk_dma2d_ch_t dma_ch, void *src_buf, void *dts_buf, uint16_t img_width, uint16_t img_height)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t size_byte = 0;
    uint32_t buf_offset = 0;
    uint32_t finish_cnt = 0;

    csk_dma_2d_rotate_cfg_t gpdam2d_rotate_para= {
        .dma2d_init = {
            .dma_ch = dma_2d_ch9,
            .burst_len = dma2d_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = dma2d_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .rd_done_ack = read_done_ack_enable,
            .handshake = hs_none,
        },

        .dma2d_img_cfg = {
            .img_input_format = csk_image_format_argb,
            .img_width = 0,
            .img_height = 0,
            .img_output_fromat_transfer = csk_image_format_transfer_yuv422_cw_90,
            .img_rgb888_format = csk_image_rgb888_format,
            .image_yuv422_rotate_mode = csk_image_yuv422_clockwise_rotate,
        }
    };

    gpdam2d_rotate_para.dma2d_init.dma_ch = dma_ch;
    gpdam2d_rotate_para.dma2d_img_cfg.img_width = img_width;
    gpdam2d_rotate_para.dma2d_img_cfg.img_height = img_height;
    size_byte = img_width * img_height * 4;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* gpdma 2d init */
    //ret = DMA2D_Initialize();
    //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&gpdam2d_rotate_para.dma2d_init, gpdma_2d_rotate1_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = DMA2D_Image_Config_Extend(gpdam2d_rotate_para.dma2d_init.dma_ch, &gpdam2d_rotate_para.dma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << gpdam2d_rotate_para.dma2d_init.dma_ch);
    finish_cnt = test_gpdma_2d_rotate1_finish_cnt;

    VIDEO_LOG("in width %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9);
    VIDEO_LOG("in height %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9);
    VIDEO_LOG("out width %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9);
    VIDEO_LOG("out height %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9);
    VIDEO_LOG("step_s %d", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9);
    VIDEO_LOG("step_l0 %d", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9);

    IP_DMA2D->REG_DMA_BLOCK_LEN_CH9.bit.CFG_BLOCK_LEN_CH9 = gpdam2d_rotate_para.dma2d_img_cfg.img_width * gpdam2d_rotate_para.dma2d_img_cfg.img_height;
    IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH9.bit.CFG_IMAGE_OUT_BLOCK_LEN_CH9 = gpdam2d_rotate_para.dma2d_img_cfg.img_width * gpdam2d_rotate_para.dma2d_img_cfg.img_height;
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9 = gpdam2d_rotate_para.dma2d_img_cfg.img_width;
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9 = gpdam2d_rotate_para.dma2d_img_cfg.img_height;
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9 = gpdam2d_rotate_para.dma2d_img_cfg.img_width;
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9 = gpdam2d_rotate_para.dma2d_img_cfg.img_height;
    IP_DMA2D->REG_DMA_IMAGE_FORMAT.bit.CFG_YUV_INPUT_FORMAT_CH9 = 3; // 0:422 1:420 2:444 3:argb8888

    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_RGB_BYPASS = (1<<3);   // ch9
    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_YUV422_BYPASS = (1<<3); // ch9
    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS1.bit.CFG_YUV_UNPACK_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_ENC_OUT2D_BYPASS.bit.CFG_ENC_OUT2D_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_DEC_OUT2D_BYPASS.bit.CFG_DEC_OUT2D_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_DEC_IN2D_BYPASS.bit.CFG_DEC_IN2D_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_ZOOM_MODE.bit.CFG_ZOOM_OUT_MODE_CH9 = 0;
    IP_DMA2D->REG_DMA_CH9_CTRL.bit.CFG_READ_DONE_ACK_EN_CH9 = 1;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_BYPASS_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_SW_CTRL_CH9 = 1;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_WNUM_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_HNUM_CH9 = gpdam2d_rotate_para.dma2d_img_cfg.img_height;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = 0 - gpdam2d_rotate_para.dma2d_img_cfg.img_width * 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9.bit.CFG_D2_ADDR_STEP_L1_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9.bit.CFG_D2_ADDR_STEP_L2_CH9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 2;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM1_CH9 = gpdam2d_rotate_para.dma2d_img_cfg.img_width;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM2_CH9 = 1;

    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_ROTA_MODE_SEL_9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_YUV2Y_EN_9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_RGB2Y_EN_9 = 0;

    buf_offset = (gpdam2d_rotate_para.dma2d_img_cfg.img_height - 1) * gpdam2d_rotate_para.dma2d_img_cfg.img_width * 4;
    ret = DMA2D_Start_Normal(gpdam2d_rotate_para.dma2d_init.dma_ch, src_buf + buf_offset, dts_buf, size_byte);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(finish_cnt, test_gpdma_2d_rotate1_finish_cnt, timeout, error);
    finish_cnt = test_gpdma_2d_rotate1_finish_cnt;
    VIDEO_LOG("finish_cnt=%d", finish_cnt);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = SUCCESS;

error:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_2d_argb8888_rotate90_00(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint8_t *argb8888_buf = NULL;
    uint8_t *argb8888_rotate_buf = NULL;
    uint32_t size_byte = 0;
    uint32_t buf_offset = 0;
    uint32_t rotate1_cnt = 0;

    uint16_t img_width = 320;
    uint16_t img_height = 240;
    size_byte = img_width * img_height * 4;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    argb8888_buf = tiny_malloc(size_byte);
    CHECK_POINT_NOT_NULL_EXIT(argb8888_buf, error1);
    VIDEO_LOG("argb8888_buf=0x%08x size=0x%x byte", argb8888_buf, size_byte);

    argb8888_rotate_buf = tiny_malloc(size_byte);
    CHECK_POINT_NOT_NULL_EXIT(argb8888_rotate_buf, error1);
    VIDEO_LOG("argb8888_rotate_buf=0x%08x size=0x%x byte", argb8888_rotate_buf, size_byte);

    argb8888_colorbar_create(argb8888_buf, img_width, img_height, 20);
    memset(argb8888_rotate_buf, 0, size_byte);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    ret = dma2d_argb8888_rotate90(dma_2d_ch9, argb8888_buf, argb8888_rotate_buf, img_width, img_height);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error2);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    while(1);

    ret = SUCCESS;

error2:
error1:
    tiny_free(argb8888_buf);
    tiny_free(argb8888_rotate_buf);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_2d_rgb565_rotate90(void)   // FAIL
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint8_t *rgb565_buf = NULL;
    uint8_t *rgb565_rotate_buf = NULL;
    uint8_t *argb8888_buf = NULL;
    uint8_t *argb8888_rotate_buf = NULL;
    uint32_t rgb565_size_byte = 0;
    uint32_t argb8888_size_byte = 0;
    uint32_t buf_offset = 0;
    uint32_t rotate1_cnt = 0;

    uint16_t img_width = 128;
    uint16_t img_height = 60;
    rgb565_size_byte = img_width * img_height * 2;
    argb8888_size_byte = img_width * img_height * 4;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    rgb565_buf = tiny_malloc(rgb565_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(rgb565_buf, error1);
    VIDEO_LOG("rgb565_buf=0x%08x size=0x%x byte", rgb565_buf, rgb565_size_byte);

    rgb565_rotate_buf = tiny_malloc(rgb565_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(rgb565_rotate_buf, error1);
    VIDEO_LOG("rgb565_rotate_buf=0x%08x size=0x%x byte", rgb565_rotate_buf, rgb565_size_byte);

    argb8888_buf = tiny_malloc(argb8888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(argb8888_buf, error1);
    VIDEO_LOG("argb8888_buf=0x%08x size=0x%x byte", argb8888_buf, argb8888_size_byte);

    argb8888_rotate_buf = tiny_malloc(argb8888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(argb8888_rotate_buf, error1);
    VIDEO_LOG("argb8888_rotate_buf=0x%08x size=0x%x byte", argb8888_rotate_buf, argb8888_size_byte);

    rgb565_colorbar_create((uint16_t *)rgb565_buf, img_width, img_height, 20);
    memset(rgb565_rotate_buf, 0, rgb565_size_byte);
    memset(argb8888_buf, 0, argb8888_size_byte);
    memset(argb8888_rotate_buf, 0, argb8888_size_byte);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    //test_2d_dts_step(dma_ch, format, src_buf, dts_buf, dts_width, src_width, src_height)
    //test_2d_src_step(dma_ch, format, src_buf, dts_buf, src_width, dts_width, dts_height)

    ret = test_2d_dts_step(dma_2d_ch9, DMA2D_FORMAT_RGB565, rgb565_buf, argb8888_buf, 4, 2, img_width * img_height / 2);
    //ret = test_2d_src_step(dma_2d_ch6, DMA2D_FORMAT_RGB565, rgb565_buf, argb8888_buf, 4, 2, img_width * img_height / 2);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1);

    ret = dma2d_argb8888_rotate90(dma_2d_ch9, argb8888_buf, argb8888_rotate_buf, img_width, img_height);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = test_2d_src_step(dma_2d_ch9, DMA2D_FORMAT_RGB565, argb8888_rotate_buf, rgb565_rotate_buf, 2, 1, img_width * img_height);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1);

    ret = SUCCESS;

error2:
error1:
    tiny_free(argb8888_buf);
    tiny_free(argb8888_rotate_buf);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static volatile uint32_t test_gpdma_2d_copy_finish_cnt = 0;

static void gpdma_2d_copy_callback(uint32_t event, void* workspace)
{
    test_gpdma_2d_copy_finish_cnt++;
}

/* buf0 -> buf1 */
static int32_t test_2d_rgb888_copy0(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t copy_cnt = 0;
    uint8_t *buf0 = NULL;
    uint8_t *buf1 = NULL;
    uint32_t rgb888_size_byte = 0;

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch9,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    };

    csk_dma_2d_image_cfg_t dma2d_img_cfg = {
        .img_input_format = csk_image_format_xrgb,
        .img_width = 320,
        .img_height = 240,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 100,
        .end_row = 100,
        .img_output_fromat_transfer = csk_image_format_transfer_xrgb_crop,
        .img_rgb888_format = csk_image_rgb888_format,
    };

    rgb888_size_byte = dma2d_img_cfg.img_width * dma2d_img_cfg.img_height * 3;   // RGB888
    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    buf0 = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf0, error1);
    VIDEO_LOG("buf0=0x%08x size=0x%x byte", buf0, rgb888_size_byte);

    buf1 = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf1, error1);
    VIDEO_LOG("buf1=0x%08x size=0x%x byte", buf1, rgb888_size_byte);

    rgb888_colorbar_create(buf0, dma2d_img_cfg.img_width, dma2d_img_cfg.img_height, 40);
    rgb888_line_color(buf1, 0, dma2d_img_cfg.img_width, 0, dma2d_img_cfg.img_height, dma2d_img_cfg.img_width, RGB888_BRED);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&dma2d_para, gpdma_2d_copy_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    mmio_write32(GPDMA_BASE + 0x1B4, (1 << dma_2d_ch9));
    copy_cnt = test_gpdma_2d_copy_finish_cnt;

    ret = DMA2D_Start_Normal(dma_2d_ch9, buf0, buf1, rgb888_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(copy_cnt, test_gpdma_2d_copy_finish_cnt, timeout, error2);
    copy_cnt = test_gpdma_2d_copy_finish_cnt;
    VIDEO_LOG("copy_cnt=%d", copy_cnt);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    while(1);

    ret = SUCCESS;

error2:
error1:
    tiny_free(buf0);
    tiny_free(buf1);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_2d_rgb565_copy0(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t copy_cnt = 0;
    uint8_t *buf0 = NULL;
    uint8_t *buf1 = NULL;
    uint32_t rgb565_size_byte = 0;
    uint32_t buf_offset = 0;

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch9,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    };

    csk_dma_2d_image_cfg_t dma2d_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = 320,
        .img_height = 240,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 100,
        .end_row = 100,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    };

    rgb565_size_byte = dma2d_img_cfg.img_width * dma2d_img_cfg.img_height * RGB565_PIXEL_BYTE;   // RGB565
    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    buf0 = tiny_malloc(rgb565_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf0, error1);
    VIDEO_LOG("buf0=0x%08x size=0x%x byte", buf0, rgb565_size_byte);

    buf1 = tiny_malloc(rgb565_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf1, error1);
    VIDEO_LOG("buf1=0x%08x size=0x%x byte", buf1, rgb565_size_byte);

    rgb565_colorbar_create((uint16_t *)buf0, dma2d_img_cfg.img_width, dma2d_img_cfg.img_height, 40);
    rgb565_line_color((uint16_t *)buf1, 0, dma2d_img_cfg.img_width, 0, dma2d_img_cfg.img_height, dma2d_img_cfg.img_width, RGB565_BRED);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&dma2d_para, gpdma_2d_copy_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << dma_2d_ch9);
    copy_cnt = test_gpdma_2d_copy_finish_cnt;

    buf_offset = (dma2d_img_cfg.img_width * (dma2d_img_cfg.start_row - 1 - 20) + (dma2d_img_cfg.start_col - 1 - 20)) * RGB565_PIXEL_BYTE;  // RGB565
    VIDEO_LOG("buf_offset=%d", buf_offset);

    ret = DMA2D_Start_Normal(dma_2d_ch9, buf0 + buf_offset, buf1, rgb565_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(copy_cnt, test_gpdma_2d_copy_finish_cnt, timeout, error2);
    copy_cnt = test_gpdma_2d_copy_finish_cnt;
    VIDEO_LOG("copy_cnt=%d", copy_cnt);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    while(1);

    ret = SUCCESS;

error2:
error1:
    tiny_free(buf0);
    tiny_free(buf1);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_2d_rgb565_copy1(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t copy_cnt = 0;
    uint8_t *buf0 = NULL;
    uint8_t *buf1 = NULL;
    uint32_t rgb565_size_byte = 0;

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch9,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    };

    csk_dma_2d_image_cfg_t dma2d_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = 320,
        .img_height = 240,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 100,
        .end_row = 100,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    };

    rgb565_size_byte = dma2d_img_cfg.img_width * dma2d_img_cfg.img_height * RGB565_PIXEL_BYTE;
    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    buf0 = tiny_malloc(rgb565_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf0, error1);
    VIDEO_LOG("buf0=0x%08x size=0x%x byte", buf0, rgb565_size_byte);

    buf1 = tiny_malloc(rgb565_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf1, error1);
    VIDEO_LOG("buf1=0x%08x size=0x%x byte", buf1, rgb565_size_byte);

    rgb565_colorbar_create((uint16_t *)buf0, dma2d_img_cfg.img_width, dma2d_img_cfg.img_height, 40);
    rgb565_line_color((uint16_t *)buf1, 0, dma2d_img_cfg.img_width, 0, dma2d_img_cfg.img_height, dma2d_img_cfg.img_width, RGB565_BRED);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&dma2d_para, gpdma_2d_copy_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(dma2d_para.dma_ch, &dma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << dma2d_para.dma_ch);
    copy_cnt = test_gpdma_2d_copy_finish_cnt;

    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_RGB_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_CH9_CTRL.bit.CFG_READ_DONE_ACK_EN_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_BYPASS_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_SW_CTRL_CH9 = 1;
    IP_DMA2D->REG_DMA_ZOOM_MODE.bit.CFG_ZOOM_OUT_MODE_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_YUV422_BYPASS = 0xF;

    IP_DMA2D->REG_DMA_DEC_IN2D_BYPASS.bit.CFG_DEC_IN2D_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS = 0x0;   // 0x0
    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS1.bit.CFG_YUV_UNPACK_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_ENC_OUT2D_BYPASS.bit.CFG_ENC_OUT2D_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_DEC_OUT2D_BYPASS.bit.CFG_DEC_OUT2D_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_ROTA_MODE_SEL_9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_YUV2Y_EN_9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_RGB2Y_EN_9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9 = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1);
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9 = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1);
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_WNUM_CH9 = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1) * RGB565_PIXEL_BYTE / 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_HNUM_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9 = ((dma2d_img_cfg.img_width - (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1)) * RGB565_PIXEL_BYTE) + 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9.bit.CFG_D2_ADDR_STEP_L1_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9.bit.CFG_D2_ADDR_STEP_L2_CH9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM1_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM2_CH9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN9 = 1;

    VIDEO_LOG("in width %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9);
    VIDEO_LOG("in height %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9);
    VIDEO_LOG("out width %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9);
    VIDEO_LOG("out height %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9);
    VIDEO_LOG("step_s %d", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9);
    VIDEO_LOG("step_l0 %d", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9);

    ret = DMA2D_Start_Normal(dma_2d_ch9, buf0, buf1, rgb565_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(copy_cnt, test_gpdma_2d_copy_finish_cnt, timeout, error2);
    copy_cnt = test_gpdma_2d_copy_finish_cnt;
    VIDEO_LOG("copy_cnt=%d", copy_cnt);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    while(1);

    ret = SUCCESS;

error2:
error1:
    tiny_free(buf0);
    tiny_free(buf1);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* width or height < 8192 */
static int32_t test_2d_src_step(csk_dma2d_ch_t dma_ch, csk_dma2d_format_t format, void *src_buf, void *dts_buf, uint16_t src_width, uint16_t dts_width, uint16_t dts_height)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t finish_cnt = 0;
    uint32_t size_byte = 0;

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch9,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    };

    csk_dma_2d_image_cfg_t dma2d_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = 320,
        .img_height = 240,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 100,
        .end_row = 100,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    };

    dma2d_para.dma_ch = dma_ch;
    dma2d_img_cfg.img_width = src_width;
    dma2d_img_cfg.img_height = dts_height;
    dma2d_img_cfg.end_col = dts_width;
    dma2d_img_cfg.end_row = dts_height;

    if(format == DMA2D_FORMAT_RGB565){
        dma2d_img_cfg.img_input_format = csk_image_format_yuv422;
        dma2d_img_cfg.img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop;
        dma2d_img_cfg.img_yuv422_format = csk_image_yuv422_format_y0cby1cr;
        size_byte = dma2d_img_cfg.img_width * dma2d_img_cfg.img_height * RGB565_PIXEL_BYTE;
    } else {  // RGB888
        dma2d_img_cfg.img_input_format = csk_image_format_xrgb;
        dma2d_img_cfg.img_output_fromat_transfer = csk_image_format_transfer_xrgb_crop;
        dma2d_img_cfg.img_rgb888_format = csk_image_rgb888_format;
        size_byte = dma2d_img_cfg.img_width * dma2d_img_cfg.img_height * RGB888_PIXEL_BYTE;
    }
    //VIDEO_LOG("[%s:%d] test start, size_byte=0x%x", __func__, __LINE__, size_byte);

    /* gpdma 2d init */
    //ret = DMA2D_Initialize();
    //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&dma2d_para, gpdma_2d_copy_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = DMA2D_Image_Config_Extend(dma2d_para.dma_ch, &dma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << dma2d_para.dma_ch);
    finish_cnt = test_gpdma_2d_copy_finish_cnt;

//    buf_offset = (dma2d_img_cfg.img_width * (dma2d_img_cfg.start_row - 1) + (dma2d_img_cfg.start_col - 1)) * RGB888_PIXEL_BYTE;
//    VIDEO_LOG("buf_offset=%d", buf_offset);

    ret = DMA2D_Start_Normal(dma2d_para.dma_ch, src_buf, dts_buf, size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(finish_cnt, test_gpdma_2d_copy_finish_cnt, timeout, error);
    finish_cnt = test_gpdma_2d_copy_finish_cnt;
    //VIDEO_LOG("finish_cnt=%d", finish_cnt);

    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = SUCCESS;

error:
    return ret;
}


/* width or height < 8192 */
static int32_t test_2d_dts_step(csk_dma2d_ch_t dma_ch, csk_dma2d_format_t format, void *src_buf, void *dts_buf, uint16_t dts_width, uint16_t src_width, uint16_t src_height)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t finish_cnt = 0;
    uint32_t size_byte = 0;

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch9,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    };

    csk_dma_2d_image_cfg_t dma2d_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = 320,
        .img_height = 240,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 100,
        .end_row = 100,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    };

    dma2d_para.dma_ch = dma_ch;
    dma2d_img_cfg.img_width = dts_width;
    dma2d_img_cfg.img_height = src_height;
    dma2d_img_cfg.end_col = src_width;
    dma2d_img_cfg.end_row = src_height;

    if(format == DMA2D_FORMAT_RGB565){
        dma2d_img_cfg.img_input_format = csk_image_format_yuv422;
        dma2d_img_cfg.img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop;
        dma2d_img_cfg.img_yuv422_format = csk_image_yuv422_format_y0cby1cr;
        size_byte = dma2d_img_cfg.img_width * dma2d_img_cfg.img_height * RGB565_PIXEL_BYTE;
    } else {  // RGB888
        dma2d_img_cfg.img_input_format = csk_image_format_xrgb;
        dma2d_img_cfg.img_output_fromat_transfer = csk_image_format_transfer_xrgb_crop;
        dma2d_img_cfg.img_rgb888_format = csk_image_rgb888_format;
        size_byte = dma2d_img_cfg.img_width * dma2d_img_cfg.img_height * RGB888_PIXEL_BYTE;
    }
    //VIDEO_LOG("[%s:%d] test start, size_byte=0x%x", __func__, __LINE__, size_byte);

    /* gpdma 2d init */
    //ret = DMA2D_Initialize();
    //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&dma2d_para, gpdma_2d_copy_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = DMA2D_Image_Config_Extend(dma2d_para.dma_ch, &dma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << dma2d_para.dma_ch);
    finish_cnt = test_gpdma_2d_copy_finish_cnt;

    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_RGB_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_CH9_CTRL.bit.CFG_READ_DONE_ACK_EN_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_BYPASS_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_SW_CTRL_CH9 = 1;
    IP_DMA2D->REG_DMA_ZOOM_MODE.bit.CFG_ZOOM_OUT_MODE_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_YUV422_BYPASS = 0xF;

    IP_DMA2D->REG_DMA_DEC_IN2D_BYPASS.bit.CFG_DEC_IN2D_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS = 0x0;   // 0x0
    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS1.bit.CFG_YUV_UNPACK_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_ENC_OUT2D_BYPASS.bit.CFG_ENC_OUT2D_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_DEC_OUT2D_BYPASS.bit.CFG_DEC_OUT2D_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_ROTA_MODE_SEL_9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_YUV2Y_EN_9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_RGB2Y_EN_9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9 = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1);
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9 = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1);
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_WNUM_CH9 = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1) * RGB565_PIXEL_BYTE / 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_HNUM_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9 = ((dma2d_img_cfg.img_width - (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1)) * RGB565_PIXEL_BYTE) + 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9.bit.CFG_D2_ADDR_STEP_L1_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9.bit.CFG_D2_ADDR_STEP_L2_CH9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM1_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM2_CH9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN9 = 1;

//    buf_offset = (dma2d_img_cfg.img_width * (dma2d_img_cfg.start_row - 1) + (dma2d_img_cfg.start_col - 1)) * RGB888_PIXEL_BYTE;
//    VIDEO_LOG("buf_offset=%d", buf_offset);

    ret = DMA2D_Start_Normal(dma2d_para.dma_ch, src_buf, dts_buf, size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(finish_cnt, test_gpdma_2d_copy_finish_cnt, timeout, error);
    finish_cnt = test_gpdma_2d_copy_finish_cnt;
    //VIDEO_LOG("finish_cnt=%d", finish_cnt);

    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = SUCCESS;

error:
    return ret;
}


static int32_t test_2d_rgb565_copy00(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t copy_cnt = 0;
    uint8_t *src_buf = NULL;
    uint8_t *dts_buf = NULL;
    uint32_t buf_offset = 0;
    uint32_t src_size_byte = 0;
    uint32_t dts_size_byte = 0;

    /* RGB565  src:320x240 -> dts:100x100 */
    csk_dma2d_ch_t dma_ch = dma_2d_ch9;
    csk_dma2d_format_t format = DMA2D_FORMAT_RGB565;   // RGB565 or RGB888
    uint16_t img_width = 320;
    uint16_t img_height = 240;
    uint16_t start_width = 60;
    uint16_t start_height = 20;
    uint16_t copy_width = 100;
    uint16_t copy_height = 100;

    if(format == DMA2D_FORMAT_RGB565) {
        src_size_byte = img_width * img_height * RGB565_PIXEL_BYTE;
        dts_size_byte = copy_width * copy_height * RGB565_PIXEL_BYTE;
        buf_offset = (img_width * start_height + start_width) * RGB565_PIXEL_BYTE;
    } else {  // RGB888
        src_size_byte = img_width * img_height * RGB888_PIXEL_BYTE;
        dts_size_byte = copy_width * copy_height * RGB888_PIXEL_BYTE;
        buf_offset = (img_width * start_height + start_width) * RGB888_PIXEL_BYTE;
    }
    VIDEO_LOG("[%s:%d] src_size_byte=0x%x", __func__, __LINE__, src_size_byte);
    VIDEO_LOG("[%s:%d] dts_size_byte=0x%x", __func__, __LINE__, dts_size_byte);
    VIDEO_LOG("buf_offset=%d", buf_offset);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    src_buf = tiny_malloc(src_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(src_buf, error);
    VIDEO_LOG("src_buf=0x%08x size=0x%x byte", src_buf, src_size_byte);

    dts_buf = tiny_malloc(dts_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dts_buf, error);
    VIDEO_LOG("dts_buf=0x%08x size=0x%x byte", dts_buf, dts_size_byte);

    if(format == DMA2D_FORMAT_RGB565){
        rgb565_colorbar_create((uint16_t *)src_buf, img_width, img_height, 40);
        rgb565_line_color((uint16_t *)dts_buf, 0, copy_width, 0, copy_height, copy_width, RGB565_BRED);
    } else {  // RGB888
        rgb888_colorbar_create(src_buf, img_width, img_height, 40);
        rgb888_line_color(dts_buf, 0, copy_width, 0, copy_height, copy_width, RGB888_BRED);
    }
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = test_2d_src_step(dma_ch, format, src_buf + buf_offset, dts_buf, img_width, copy_width, copy_height);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    while(1);

    ret = SUCCESS;

error:
    tiny_free(src_buf);
    tiny_free(dts_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_2d_rgb565_copy11(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t copy_cnt = 0;
    uint8_t *src_buf = NULL;
    uint8_t *dts_buf = NULL;
    uint32_t buf_offset = 0;
    uint32_t src_size_byte = 0;
    uint32_t dts_size_byte = 0;

    /* RGB565  src:100x100 -> dts:320x240 */
    csk_dma2d_ch_t dma_ch = dma_2d_ch9;
    csk_dma2d_format_t format = DMA2D_FORMAT_RGB565;   // RGB565 or RGB888
    uint16_t img_width = 320;
    uint16_t img_height = 240;
    uint16_t start_width = 60;
    uint16_t start_height = 20;
    uint16_t copy_width = 100;
    uint16_t copy_height = 100;

    if(format == DMA2D_FORMAT_RGB565) {
        src_size_byte = copy_width * copy_height * RGB565_PIXEL_BYTE;
        dts_size_byte = img_width * img_height * RGB565_PIXEL_BYTE;
        buf_offset = (img_width * start_height + start_width) * RGB565_PIXEL_BYTE;
    } else {  // RGB888
        src_size_byte = copy_width * copy_height * RGB888_PIXEL_BYTE;
                dts_size_byte = img_width * img_height * RGB888_PIXEL_BYTE;
        buf_offset = (img_width * start_height + start_width) * RGB888_PIXEL_BYTE;
    }
    VIDEO_LOG("[%s:%d] src_size_byte=0x%x", __func__, __LINE__, src_size_byte);
    VIDEO_LOG("[%s:%d] dts_size_byte=0x%x", __func__, __LINE__, dts_size_byte);
    VIDEO_LOG("buf_offset=%d", buf_offset);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    src_buf = tiny_malloc(src_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(src_buf, error);
    VIDEO_LOG("src_buf=0x%08x size=0x%x byte", src_buf, src_size_byte);

    dts_buf = tiny_malloc(dts_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dts_buf, error);
    VIDEO_LOG("dts_buf=0x%08x size=0x%x byte", dts_buf, dts_size_byte);

    if(format == DMA2D_FORMAT_RGB565){
        rgb565_colorbar_create((uint16_t *)src_buf, copy_width, copy_height, 20);
        rgb565_line_color((uint16_t *)dts_buf, 0, img_width, 0, img_height, img_width, RGB565_BRED);
    } else {  // RGB888
        rgb888_colorbar_create(src_buf, copy_width, copy_height, 20);
        rgb888_line_color(dts_buf, 0, img_width, 0, img_height, img_width, RGB888_BRED);
    }
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = test_2d_dts_step(dma_ch, format, src_buf, dts_buf + buf_offset, img_width, copy_width, copy_height);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    while(1);

    ret = SUCCESS;

error:
    tiny_free(src_buf);
    tiny_free(dts_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_2d_rgb565_copy22(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t finish_cnt = 0;
    uint8_t *src_buf = NULL;
    uint8_t *copy_buf = NULL;
    uint8_t *dts_buf = NULL;
    uint32_t src_offset = 0;
    uint32_t dts_offset = 0;
    uint32_t src_size_byte = 0;
    uint32_t copy_size_byte = 0;
    uint32_t dts_size_byte = 0;

    /* RGB565  src:320x240 -> dts:100x100 */
    csk_dma2d_ch_t dma_ch = dma_2d_ch9;
    csk_dma2d_format_t format = DMA2D_FORMAT_RGB565;   // RGB565 or RGB888
    uint16_t src_img_width = 320;
    uint16_t src_img_height = 240;
    uint16_t src_start_width = 60;
    uint16_t src_start_height = 20;
    uint16_t dts_img_width = 400;
    uint16_t dts_img_height = 500;
    uint16_t dts_start_width = 80;
    uint16_t dts_start_height = 100;
    uint16_t copy_width = 100;
    uint16_t copy_height = 100;

    if(format == DMA2D_FORMAT_RGB565) {
        src_size_byte = src_img_width * src_img_height * RGB565_PIXEL_BYTE;
        copy_size_byte = copy_width * copy_height * RGB565_PIXEL_BYTE;
        dts_size_byte = dts_img_width * dts_img_height * RGB565_PIXEL_BYTE;
        src_offset = (src_img_width * src_start_height + src_start_width) * RGB565_PIXEL_BYTE;
        dts_offset = (dts_img_width * dts_start_height + dts_start_width) * RGB565_PIXEL_BYTE;
    } else {  // RGB888
        src_size_byte = src_img_width * src_img_height * RGB888_PIXEL_BYTE;
        copy_size_byte = copy_width * copy_height * RGB888_PIXEL_BYTE;
        dts_size_byte = dts_img_width * dts_img_height * RGB888_PIXEL_BYTE;
        src_offset = (src_img_width * src_start_height + src_start_width) * RGB888_PIXEL_BYTE;
        dts_offset = (dts_img_width * dts_start_height + dts_start_width) * RGB888_PIXEL_BYTE;
    }
    VIDEO_LOG("[%s:%d] src_size_byte=0x%x", __func__, __LINE__, src_size_byte);
    VIDEO_LOG("[%s:%d] copy_size_byte=0x%x", __func__, __LINE__, copy_size_byte);
    VIDEO_LOG("[%s:%d] dts_size_byte=0x%x", __func__, __LINE__, dts_size_byte);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    src_buf = tiny_malloc(src_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(src_buf, error);
    VIDEO_LOG("src_buf=0x%08x size=0x%x byte", src_buf, src_size_byte);

    copy_buf = tiny_malloc(copy_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(copy_buf, error);
    VIDEO_LOG("copy_buf=0x%08x size=0x%x byte", copy_buf, copy_size_byte);

    dts_buf = tiny_malloc(dts_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dts_buf, error);
    VIDEO_LOG("dts_buf=0x%08x size=0x%x byte", dts_buf, dts_size_byte);

    if(format == DMA2D_FORMAT_RGB565){
        rgb565_colorbar_create((uint16_t *)src_buf, src_img_width, src_img_height, 40);
        memset(copy_buf, 0, copy_size_byte);
        rgb565_line_color((uint16_t *)dts_buf, 0, dts_img_width, 0, dts_img_height, dts_img_width, RGB565_BRED);
    } else {  // RGB888
        rgb888_colorbar_create(src_buf, src_img_width, src_img_height, 40);
        memset(copy_buf, 0, copy_size_byte);
        rgb888_line_color(dts_buf, 0, dts_img_width, 0, dts_img_height, dts_img_width, RGB888_BRED);
    }
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = test_2d_src_step(dma_ch, format, src_buf + src_offset, copy_buf, src_img_width, copy_width, copy_height);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = test_2d_dts_step(dma_ch, format, copy_buf, dts_buf + dts_offset, dts_img_width, copy_width, copy_height);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1);

    ret = SUCCESS;

error:
    tiny_free(src_buf);
    tiny_free(copy_buf);
    tiny_free(dts_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t lv_gpu_stm32_dma2d_copy(void *buf, uint16_t buf_w, void *map, uint16_t map_w, uint16_t copy_w, uint16_t copy_h)
{
    int32_t ret = FAILURE;
    uint8_t *copy_buf = NULL;
    uint32_t copy_size_byte = 0;

    copy_size_byte = copy_w * copy_h * RGB565_PIXEL_BYTE;
    copy_buf = tiny_malloc(copy_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(copy_buf, error);
    //VIDEO_LOG("copy_buf=0x%08x size=0x%x byte", copy_buf, copy_size_byte);

    ret = test_2d_src_step(dma_2d_ch9, DMA2D_FORMAT_RGB565, map, copy_buf, map_w, copy_w, copy_h);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error);
    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = test_2d_dts_step(dma_2d_ch9, DMA2D_FORMAT_RGB565, copy_buf, buf, buf_w, copy_w, copy_h);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error);
    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = SUCCESS;

error:
    tiny_free(copy_buf);
    return ret;
}


static int32_t test_2d_rgb565_copy(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t finish_cnt = 0;
    uint8_t *src_buf = NULL;
    uint8_t *dts_buf = NULL;
    uint32_t src_offset = 0;
    uint32_t dts_offset = 0;
    uint32_t src_size_byte = 0;
    uint32_t dts_size_byte = 0;

    /* RGB565  src:320x240 -> dts:100x100 */
    csk_dma2d_ch_t dma_ch = dma_2d_ch9;
    csk_dma2d_format_t format = DMA2D_FORMAT_RGB565;   // RGB565 or RGB888
    uint16_t src_img_width = 320;
    uint16_t src_img_height = 240;
    uint16_t src_start_width = 60;
    uint16_t src_start_height = 20;
    uint16_t dts_img_width = 400;
    uint16_t dts_img_height = 500;
    uint16_t dts_start_width = 80;
    uint16_t dts_start_height = 100;
    uint16_t copy_width = 100;
    uint16_t copy_height = 100;

    if(format == DMA2D_FORMAT_RGB565) {
        src_size_byte = src_img_width * src_img_height * RGB565_PIXEL_BYTE;
        dts_size_byte = dts_img_width * dts_img_height * RGB565_PIXEL_BYTE;
        src_offset = (src_img_width * src_start_height + src_start_width) * RGB565_PIXEL_BYTE;
        dts_offset = (dts_img_width * dts_start_height + dts_start_width) * RGB565_PIXEL_BYTE;
    } else {  // RGB888
        src_size_byte = src_img_width * src_img_height * RGB888_PIXEL_BYTE;
        dts_size_byte = dts_img_width * dts_img_height * RGB888_PIXEL_BYTE;
        src_offset = (src_img_width * src_start_height + src_start_width) * RGB888_PIXEL_BYTE;
        dts_offset = (dts_img_width * dts_start_height + dts_start_width) * RGB888_PIXEL_BYTE;
    }
    VIDEO_LOG("[%s:%d] src_size_byte=0x%x", __func__, __LINE__, src_size_byte);
    VIDEO_LOG("[%s:%d] dts_size_byte=0x%x", __func__, __LINE__, dts_size_byte);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    src_buf = tiny_malloc(src_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(src_buf, error);
    VIDEO_LOG("src_buf=0x%08x size=0x%x byte", src_buf, src_size_byte);

    dts_buf = tiny_malloc(dts_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dts_buf, error);
    VIDEO_LOG("dts_buf=0x%08x size=0x%x byte", dts_buf, dts_size_byte);

    if(format == DMA2D_FORMAT_RGB565) {
        rgb565_colorbar_create((uint16_t *)src_buf, src_img_width, src_img_height, 40);
        rgb565_line_color((uint16_t *)dts_buf, 0, dts_img_width, 0, dts_img_height, dts_img_width, RGB565_BRED);
    } else {  // RGB888
        rgb888_colorbar_create(src_buf, src_img_width, src_img_height, 40);
        rgb888_line_color(dts_buf, 0, dts_img_width, 0, dts_img_height, dts_img_width, RGB888_BRED);
    }
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = lv_gpu_stm32_dma2d_copy(dts_buf + dts_offset, dts_img_width, src_buf + src_offset, src_img_width, copy_width, copy_height);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1);

    ret = SUCCESS;

error:
    tiny_free(src_buf);
    tiny_free(dts_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static void d2blender_back_dma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
}

static void d2blender_fore_dma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
}

static volatile uint32_t blender_dma_finish_flag = 0;

static void d2blender_out_dma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    blender_dma_finish_flag++;
}

static int32_t dma2d_rgb565_color_fill(void *buf, uint32_t rgb888_color, uint16_t img_width, uint16_t img_height)
{
    int32_t ret = FAILURE;
    uint32_t i = 0;
    uint32_t size_byte = 0;

    void *blender_dev = Blender0();
    static Blender_InitTypeDef blender_cfg = {
            .blender_mode = BLENDER_MODE_FILL,
            .alpha_mode = BLENDER_ALPHA_MODE_2,
            .back_format = BLENDER_BACK_FORMAT_RGB565,
            .fore_format = BLENDER_FORE_FORMAT_RGB565,
            .img_width = 0,
            .img_height = 0,
            .color = 0x808080,
            .alpha = 0xFF,
            .burst_thd = 8,
    };

    static csk_gpdma_init_t d2back_input = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2p,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_fix,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = d2back_hs_num3,
    };

    static csk_gpdma_init_t d2out_output = {
            .dma_ch = gp_dma_ch1,
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

    blender_cfg.color = rgb888_color;
    blender_cfg.img_width = img_width;
    blender_cfg.img_height = img_height;
    size_byte = img_width * img_height * 2;  // RGB565

    /* blender init */
    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_BLENDER_CLK = 0x1; // blender clk enable
    //IP_AP_CFG->REG_SW_RESET.bit.VIDEO_RESET = 0x1;
    //IP_AP_CFG->REG_SW_RESET.bit.BLENDER_RESET = 0x1;
    //DELAY_MS(1);

    ret = Blender_Initialize(blender_dev, &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    /* GPDMA init */
    ret = GPDMA_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    ret = GPDMA_Config(&d2back_input, d2blender_back_dma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    ret = GPDMA_Config(&d2out_output, d2blender_out_dma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* GPDMA start */
    blender_dma_finish_flag = 0;
    ret = GPDMA_Start_Normal(d2back_input.dma_ch, buf, (uint32_t*)D2BACK_BUF, size_byte);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    ret = GPDMA_Start_Normal(d2out_output.dma_ch, (uint32_t*)D2OUT_BUF, buf, size_byte);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* blender start */
    Blender_Start(blender_dev);

    i = 1000000;  // wait blender done, timeout=1000ms
    while(!blender_dma_finish_flag)
    {
        DELAY_US(1);
        if(i-- == 0)
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


static int32_t lv_gpu_stm32_dma2d_fill(void *buf, uint16_t buf_w, uint32_t color, uint16_t fill_w, uint16_t fill_h)
{
    int32_t ret = FAILURE;
    uint8_t *color_buf = NULL;
    uint32_t color_size_byte = 0;
    uint32_t rgb888 = 0;

    color_size_byte = fill_w * fill_h * RGB565_PIXEL_BYTE;
    color_buf = tiny_malloc(color_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(color_buf, error);
    //VIDEO_LOG("color_buf=0x%08x size=0x%x byte", color_buf, color_size_byte);

    rgb888 = ((((color & 0xF800) >> 8) | ((color & 0xF800) >> 13)) << 16) | \
             ((((color & 0x07E0) >> 3) | ((color & 0x07E0) >> 9)) << 8 ) | \
             (((color & 0x001F) << 3) | ((color & 0x001F) >> 2));
    ret = dma2d_rgb565_color_fill(color_buf, rgb888, fill_w, fill_h);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error);

    ret = test_2d_dts_step(dma_2d_ch9, DMA2D_FORMAT_RGB565, color_buf, buf, buf_w, fill_w, fill_h);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error);
    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = SUCCESS;

error:
    tiny_free(color_buf);
    return ret;
}


static int32_t test_2d_rgb565_fill(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t finish_cnt = 0;
    uint8_t *img_buf = NULL;
    uint32_t img_offset = 0;
    uint32_t img_size_byte = 0;

    /* RGB565  src:320x240 -> dts:100x100 */
    csk_dma2d_ch_t dma_ch = dma_2d_ch9;
    csk_dma2d_format_t format = DMA2D_FORMAT_RGB565;   // RGB565 or RGB888
    uint16_t img_width = 320;
    uint16_t img_height = 240;
    uint16_t start_width = 60;
    uint16_t start_height = 20;
    uint16_t color_width = 100;
    uint16_t color_height = 10;
    uint32_t color = RGB565_RED;      // 0xF800

    if(format == DMA2D_FORMAT_RGB565) {
        img_size_byte = img_width * img_height * RGB565_PIXEL_BYTE;
        img_offset = (img_width * start_height + start_width) * RGB565_PIXEL_BYTE;
    } else {  // RGB888
        img_size_byte = img_width * img_height * RGB888_PIXEL_BYTE;
        img_offset = (img_width * start_height + start_width) * RGB888_PIXEL_BYTE;
    }
    VIDEO_LOG("[%s:%d] img_size_byte=0x%x", __func__, __LINE__, img_size_byte);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    img_buf = tiny_malloc(img_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(img_buf, error);
    VIDEO_LOG("img_buf=0x%08x size=0x%x byte", img_buf, img_size_byte);

    if(format == DMA2D_FORMAT_RGB565) {
        rgb565_colorbar_create((uint16_t *)img_buf, img_width, img_height, 40);
    } else {  // RGB888
        rgb888_colorbar_create(img_buf, img_width, img_height, 40);
    }
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = lv_gpu_stm32_dma2d_fill(img_buf + img_offset, img_width, color, color_width, color_height);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1);

    ret = SUCCESS;

error:
    tiny_free(img_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t rgb565_color_blend(void *back_buf, void *fore_buf, void *out_buf, uint8_t opa, uint16_t img_width, uint16_t img_height)
{
    int32_t ret = FAILURE;
    uint32_t i = 0;
    uint32_t size_byte = 0;

    void *blender_dev = Blender0();
    static Blender_InitTypeDef blender_cfg = {
            .blender_mode = BLENDER_MODE_MAP,
            .alpha_mode = BLENDER_ALPHA_MODE_2,
            .back_format = BLENDER_BACK_FORMAT_RGB565,
            .fore_format = BLENDER_FORE_FORMAT_RGB565,
            .img_width = 0,
            .img_height = 0,
            .color = 0,
            .alpha = 0,
            .burst_thd = 8,
    };

    static csk_gpdma_init_t d2back_input = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2p,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_fix,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = d2back_hs_num3,
    };

    csk_gpdma_init_t d2fore_input = {
            .dma_ch = gp_dma_ch1,
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
            .dma_ch = gp_dma_ch2,
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

    blender_cfg.alpha = opa;
    blender_cfg.img_width = img_width;
    blender_cfg.img_height = img_height;
    size_byte = img_width * img_height * 2;  // RGB565

    /* blender init */
    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_BLENDER_CLK = 0x1; // blender clk enable
    //IP_AP_CFG->REG_SW_RESET.bit.VIDEO_RESET = 0x1;
    //IP_AP_CFG->REG_SW_RESET.bit.BLENDER_RESET = 0x1;
    //DELAY_MS(1);

    ret = Blender_Initialize(blender_dev, &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    /* GPDMA init */
    ret = GPDMA_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    ret = GPDMA_Config(&d2back_input, d2blender_back_dma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    ret = GPDMA_Config(&d2fore_input, d2blender_fore_dma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    ret = GPDMA_Config(&d2out_output, d2blender_out_dma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* GPDMA start */
    blender_dma_finish_flag = 0;
    ret = GPDMA_Start_Normal(d2back_input.dma_ch, back_buf, (uint32_t*)D2BACK_BUF, size_byte);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    ret = GPDMA_Start_Normal(d2fore_input.dma_ch, fore_buf, (uint32_t*)D2FORE_BUF, size_byte);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    ret = GPDMA_Start_Normal(d2out_output.dma_ch, (uint32_t*)D2OUT_BUF, out_buf, size_byte);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* blender start */
    Blender_Start(blender_dev);

    i = 1000000;  // wait blender done, timeout=1000ms
    while(!blender_dma_finish_flag)
    {
        DELAY_US(1);
        if(i-- == 0)
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


static int32_t lv_gpu_stm32_dma2d_blend(void *buf, uint16_t buf_w, void *map, uint8_t opa, uint16_t map_w, uint16_t copy_w, uint16_t copy_h)
{
    int32_t ret = FAILURE;
    uint8_t *back_buf = NULL;
    uint8_t *fore_buf = NULL;
    uint8_t *out_buf = NULL;
    uint32_t copy_size_byte = 0;

    copy_size_byte = copy_w * copy_h * RGB565_PIXEL_BYTE;
    back_buf = tiny_malloc(copy_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(back_buf, error);
    //VIDEO_LOG("back_buf=0x%08x size=0x%x byte", back_buf, copy_size_byte);
    fore_buf = tiny_malloc(copy_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(fore_buf, error);
    //VIDEO_LOG("fore_buf=0x%08x size=0x%x byte", fore_buf, copy_size_byte);
    out_buf = tiny_malloc(copy_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(out_buf, error);
    //VIDEO_LOG("out_buf=0x%08x size=0x%x byte", out_buf, copy_size_byte);

    ret = test_2d_src_step(dma_2d_ch9, DMA2D_FORMAT_RGB565, map, back_buf, map_w, copy_w, copy_h);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error);

    ret = test_2d_src_step(dma_2d_ch9, DMA2D_FORMAT_RGB565, buf, fore_buf, buf_w, copy_w, copy_h);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error);

    ret = rgb565_color_blend(back_buf, fore_buf, out_buf, opa, copy_w, copy_h);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error);

    ret = test_2d_dts_step(dma_2d_ch9, DMA2D_FORMAT_RGB565, out_buf, buf, buf_w, copy_w, copy_h);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error);

    ret = SUCCESS;

error:
    tiny_free(back_buf);
    tiny_free(fore_buf);
    tiny_free(out_buf);
    return ret;
}


static int32_t test_2d_rgb565_blend(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t finish_cnt = 0;
    uint8_t *src_buf = NULL;
    uint8_t *dts_buf = NULL;
    uint32_t src_offset = 0;
    uint32_t dts_offset = 0;
    uint32_t src_size_byte = 0;
    uint32_t dts_size_byte = 0;

    /* RGB565  src:320x240 -> dts:100x100 */
    csk_dma2d_ch_t dma_ch = dma_2d_ch9;
    csk_dma2d_format_t format = DMA2D_FORMAT_RGB565;   // RGB565 or RGB888
    uint16_t src_img_width = 64;
    uint16_t src_img_height = 40;
    uint16_t src_start_width = 16;
    uint16_t src_start_height = 8;
    uint16_t dts_img_width = 80;
    uint16_t dts_img_height = 60;
    uint16_t dts_start_width = 32;
    uint16_t dts_start_height = 20;
    uint16_t copy_width = 32;
    uint16_t copy_height = 32;
    uint8_t opa = 0x80;

    if(format == DMA2D_FORMAT_RGB565) {
        src_size_byte = src_img_width * src_img_height * RGB565_PIXEL_BYTE;
        dts_size_byte = dts_img_width * dts_img_height * RGB565_PIXEL_BYTE;
        src_offset = (src_img_width * src_start_height + src_start_width) * RGB565_PIXEL_BYTE;
        dts_offset = (dts_img_width * dts_start_height + dts_start_width) * RGB565_PIXEL_BYTE;
    } else {  // RGB888
        src_size_byte = src_img_width * src_img_height * RGB888_PIXEL_BYTE;
        dts_size_byte = dts_img_width * dts_img_height * RGB888_PIXEL_BYTE;
        src_offset = (src_img_width * src_start_height + src_start_width) * RGB888_PIXEL_BYTE;
        dts_offset = (dts_img_width * dts_start_height + dts_start_width) * RGB888_PIXEL_BYTE;
    }
    VIDEO_LOG("[%s:%d] src_size_byte=0x%x", __func__, __LINE__, src_size_byte);
    VIDEO_LOG("[%s:%d] dts_size_byte=0x%x", __func__, __LINE__, dts_size_byte);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    src_buf = tiny_malloc(src_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(src_buf, error);
    VIDEO_LOG("src_buf=0x%08x size=0x%x byte", src_buf, src_size_byte);

    dts_buf = tiny_malloc(dts_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dts_buf, error);
    VIDEO_LOG("dts_buf=0x%08x size=0x%x byte", dts_buf, dts_size_byte);

    if(format == DMA2D_FORMAT_RGB565) {
        rgb565_colorbar_create((uint16_t *)src_buf, src_img_width, src_img_height, 8);
        rgb565_line_color((uint16_t *)dts_buf, 0, dts_img_width, 0, dts_img_height, dts_img_width, RGB565_BRED);
    } else {  // RGB888
        rgb888_colorbar_create(src_buf, src_img_width, src_img_height, 8);
        rgb888_line_color(dts_buf, 0, dts_img_width, 0, dts_img_height, dts_img_width, RGB888_BRED);
    }
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = lv_gpu_stm32_dma2d_blend(dts_buf + dts_offset, dts_img_width, src_buf + src_offset, opa, src_img_width, copy_width, copy_height);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1);

    ret = SUCCESS;

error:
    tiny_free(src_buf);
    tiny_free(dts_buf);

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* buf0 -> buf1    GPDMA2D  ->  BLENDER  ->  GPDMA2D */
static int32_t test_2d_rgb888_copy1(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t copy_cnt = 0;
    uint8_t *buf0 = NULL;
    uint8_t *buf1 = NULL;
    uint32_t rgb888_size_byte = 0;

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch9,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    };

    csk_dma_2d_image_cfg_t dma2d_img_cfg = {
        .img_input_format = csk_image_format_xrgb,
        .img_width = 320,
        .img_height = 240,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 100,
        .end_row = 100,
        .img_output_fromat_transfer = csk_image_format_transfer_xrgb_crop,
        .img_rgb888_format = csk_image_rgb888_format,
    };

    rgb888_size_byte = dma2d_img_cfg.img_width * dma2d_img_cfg.img_height * RGB888_PIXEL_BYTE;   // RGB888
    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    buf0 = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf0, error1);
    VIDEO_LOG("buf0=0x%08x size=0x%x byte", buf0, rgb888_size_byte);

    buf1 = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf1, error1);
    VIDEO_LOG("buf1=0x%08x size=0x%x byte", buf1, rgb888_size_byte);

    rgb888_colorbar_create(buf0, dma2d_img_cfg.img_width, dma2d_img_cfg.img_height, 40);
    rgb888_line_color(buf1, 0, dma2d_img_cfg.img_width, 0, dma2d_img_cfg.img_height, dma2d_img_cfg.img_width, RGB888_BRED);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&dma2d_para, gpdma_2d_copy_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << dma_2d_ch9);
    copy_cnt = test_gpdma_2d_copy_finish_cnt;

    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_RGB_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_CH9_CTRL.bit.CFG_READ_DONE_ACK_EN_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_BYPASS_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_SW_CTRL_CH9 = 1;
    IP_DMA2D->REG_DMA_ZOOM_MODE.bit.CFG_ZOOM_OUT_MODE_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_YUV422_BYPASS = 0xF;

    IP_DMA2D->REG_DMA_DEC_IN2D_BYPASS.bit.CFG_DEC_IN2D_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS = 0x0;   // 0x0
    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS1.bit.CFG_YUV_UNPACK_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_ENC_OUT2D_BYPASS.bit.CFG_ENC_OUT2D_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_DEC_OUT2D_BYPASS.bit.CFG_DEC_OUT2D_BYPASS = 0xF;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_ROTA_MODE_SEL_9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_YUV2Y_EN_9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_RGB2Y_EN_9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9 = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1);
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9 = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1);
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_WNUM_CH9 = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1) * RGB888_PIXEL_BYTE / 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_HNUM_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9 = ((dma2d_img_cfg.img_width - (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1)) * RGB888_PIXEL_BYTE) + 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9.bit.CFG_D2_ADDR_STEP_L1_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9.bit.CFG_D2_ADDR_STEP_L2_CH9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM1_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM2_CH9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN9 = 1;

    VIDEO_LOG("in width %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9);
    VIDEO_LOG("in height %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9);
    VIDEO_LOG("out width %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9);
    VIDEO_LOG("out height %d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9);
    VIDEO_LOG("step_s %d", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9);
    VIDEO_LOG("step_l0 %d", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9);

    ret = DMA2D_Start_Normal(dma_2d_ch9, buf0, buf1, rgb888_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(copy_cnt, test_gpdma_2d_copy_finish_cnt, timeout, error2);
    copy_cnt = test_gpdma_2d_copy_finish_cnt;
    VIDEO_LOG("copy_cnt=%d", copy_cnt);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    while(1);

    ret = SUCCESS;

error2:
error1:
    tiny_free(buf0);
    tiny_free(buf1);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* buf0 -> buf1    GPDMA2D  ->  BLENDER  ->  GPDMA2D */
static int32_t test_2d_rgb888_copy2(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t copy_cnt = 0;
    uint8_t *buf0 = NULL;
    uint8_t *buf1 = NULL;
    uint32_t buf_offset = 0;
    uint32_t rgb888_size_byte = 0;
    uint32_t crop_size_byte = 0;

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch9,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    };

    csk_dma_2d_image_cfg_t dma2d_img_cfg = {
        .img_input_format = csk_image_format_xrgb,
        .img_width = 320,
        .img_height = 240,
        .start_col = (100+1),
        .start_row = (100+1),
        .end_col = 200,
        .end_row = 200,
        .img_output_fromat_transfer = csk_image_format_transfer_xrgb_crop,
        .img_rgb888_format = csk_image_rgb888_format,
    };

    rgb888_size_byte = dma2d_img_cfg.img_width * dma2d_img_cfg.img_height * RGB888_PIXEL_BYTE;   // RGB888
    crop_size_byte = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1) * (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1) * RGB888_PIXEL_BYTE;   // RGB888
    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    buf0 = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf0, error1);
    VIDEO_LOG("buf0=0x%08x size=0x%x byte", buf0, rgb888_size_byte);

    buf1 = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf1, error1);
    VIDEO_LOG("buf1=0x%08x size=0x%x byte", buf1, rgb888_size_byte);

    rgb888_colorbar_create(buf0, dma2d_img_cfg.img_width, dma2d_img_cfg.img_height, 20);
    rgb888_line_color(buf1, 0, dma2d_img_cfg.img_width, 0, dma2d_img_cfg.img_height, dma2d_img_cfg.img_width, RGB888_BRED);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&dma2d_para, gpdma_2d_copy_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << dma_2d_ch9);
    copy_cnt = test_gpdma_2d_copy_finish_cnt;

    VIDEO_LOG("step_s=%d", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9);
    VIDEO_LOG("step_l0=%d", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9);

    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_RGB_BYPASS = (1<<3);   // ch9
    IP_DMA2D->REG_DMA_CH9_CTRL.bit.CFG_READ_DONE_ACK_EN_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_BYPASS_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_SW_CTRL_CH9 = 1;
    IP_DMA2D->REG_DMA_ZOOM_MODE.bit.CFG_ZOOM_OUT_MODE_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_YUV422_BYPASS = (1<<3); // ch9

    IP_DMA2D->REG_DMA_DEC_IN2D_BYPASS.bit.CFG_DEC_IN2D_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS = 0x0;
    IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS1.bit.CFG_YUV_UNPACK_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_ENC_OUT2D_BYPASS.bit.CFG_ENC_OUT2D_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_DEC_OUT2D_BYPASS.bit.CFG_DEC_OUT2D_BYPASS = (1<<3);  // ch9
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_ROTA_MODE_SEL_9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_YUV2Y_EN_9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_RGB2Y_EN_9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9 = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1);
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9 = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1);
    IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_WNUM_CH9 = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1) * RGB888_PIXEL_BYTE / 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_HNUM_CH9 = 1;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9 = ((dma2d_img_cfg.img_width - (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1)) * RGB888_PIXEL_BYTE) + 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9.bit.CFG_D2_ADDR_STEP_L1_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9.bit.CFG_D2_ADDR_STEP_L2_CH9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM1_CH9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM2_CH9 = 0;

    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN9 = 1;

    VIDEO_LOG("in width=%d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9);
    VIDEO_LOG("in height=%d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9);
    VIDEO_LOG("out width=%d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9);
    VIDEO_LOG("out height=%d", IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9);
    VIDEO_LOG("step_s=%d", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9);
    VIDEO_LOG("step_l0=%d", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9);

    buf_offset = (dma2d_img_cfg.img_width * (dma2d_img_cfg.start_row - 1 - 20) + (dma2d_img_cfg.start_col - 1 - 20)) * RGB888_PIXEL_BYTE;
    VIDEO_LOG("buf_offset=%d", buf_offset);
    ret = DMA2D_Start_Normal(dma_2d_ch9, buf0, buf1 + buf_offset, crop_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(copy_cnt, test_gpdma_2d_copy_finish_cnt, timeout, error2);
    copy_cnt = test_gpdma_2d_copy_finish_cnt;
    VIDEO_LOG("copy_cnt=%d", copy_cnt);

    VIDEO_LOG("buf0=0x%x", buf0);
    VIDEO_LOG("buf0+offset=0x%x", buf0+buf_offset);
    VIDEO_LOG("buf1=0x%x", buf1);
    VIDEO_LOG("buf1+offset=0x%x", buf1+buf_offset);
    VIDEO_LOG("SRC_ADDR0_CH9 *0x%x=0x%x", &IP_DMA2D->REG_DMA_SRC_ADDR0_CH9.all, IP_DMA2D->REG_DMA_SRC_ADDR0_CH9.all);
    VIDEO_LOG("DST_ADDR0_CH9 *0x%x=0x%x", &IP_DMA2D->REG_DMA_DST_ADDR0_CH9.all, IP_DMA2D->REG_DMA_DST_ADDR0_CH9.all);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    while(1);

    ret = SUCCESS;

error2:
error1:
    tiny_free(buf0);
    tiny_free(buf1);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* buf0 -> buf1    GPDMA2D  ->  BLENDER  ->  GPDMA2D */
static int32_t test_2d_rgb888_copy3(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t copy_cnt = 0;
    uint8_t *buf0 = NULL;
    uint8_t *buf1 = NULL;
    uint32_t rgb888_size_byte = 0;

    csk_dma2d_init_t dma2d_para = {
        .dma_ch = dma_2d_ch9,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    };

    csk_dma_2d_image_cfg_t dma2d_img_cfg = {
        .img_input_format = csk_image_format_xrgb,
        .img_width = 320,
        .img_height = 240,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 100,
        .end_row = 100,
        .img_output_fromat_transfer = csk_image_format_transfer_xrgb_crop,
        .img_rgb888_format = csk_image_rgb888_format,
    };

    rgb888_size_byte = dma2d_img_cfg.img_width * dma2d_img_cfg.img_height * RGB888_PIXEL_BYTE;   // RGB888
    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    buf0 = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf0, error1);
    VIDEO_LOG("buf0=0x%08x size=0x%x byte", buf0, rgb888_size_byte);

    buf1 = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf1, error1);
    VIDEO_LOG("buf1=0x%08x size=0x%x byte", buf1, rgb888_size_byte);

    rgb888_colorbar_create(buf0, dma2d_img_cfg.img_width, dma2d_img_cfg.img_height, 40);
    rgb888_line_color(buf1, 0, dma2d_img_cfg.img_width, 0, dma2d_img_cfg.img_height, dma2d_img_cfg.img_width, RGB888_BRED);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&dma2d_para, gpdma_2d_copy_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << dma_2d_ch9);
    copy_cnt = test_gpdma_2d_copy_finish_cnt;

//    VIDEO_LOG("ENC_IN2D=0x%x", IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS);
//    VIDEO_LOG("CTRL1=0x%x", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.all);
//    VIDEO_LOG("CTRL3=0x%x", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.all);
//    VIDEO_LOG("IMAGE_FEATURE=0x%x", IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all);

    IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS = 0x0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9 = ((dma2d_img_cfg.img_width - (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1)) * RGB888_PIXEL_BYTE) + 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN9 = 1;

    ret = DMA2D_Start_Normal(dma_2d_ch9, buf0, buf1, rgb888_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(copy_cnt, test_gpdma_2d_copy_finish_cnt, timeout, error2);
    copy_cnt = test_gpdma_2d_copy_finish_cnt;
    VIDEO_LOG("copy_cnt=%d", copy_cnt);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    while(1);

    ret = SUCCESS;

error2:
error1:
    tiny_free(buf0);
    tiny_free(buf1);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static volatile uint32_t test_gpdma_2d_blander_in_finish_cnt = 0;
static volatile uint32_t test_gpdma_2d_blander_out_finish_cnt = 0;

static void gpdma_2d_blander_in_callback(uint32_t event, void* workspace)
{
    test_gpdma_2d_blander_in_finish_cnt++;
}

static void gpdma_2d_blander_out_callback(uint32_t event, void* workspace)
{
    test_gpdma_2d_blander_out_finish_cnt++;
}


/* buf0 -> buf1    GPDMA2D  ->  BLENDER  ->  GPDMA2D */
static int32_t test_2d_rgb888_copy4(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t copy_cnt = 0;
    uint32_t blender_in_cnt = 0;
    uint32_t blender_out_cnt = 0;
    uint8_t *buf_src = NULL;
    uint8_t *buf_dts = NULL;
    uint8_t *buf_crop_src = NULL;
    uint8_t *buf_crop_dts = NULL;
    uint32_t rgb888_size_byte = 0;
    uint32_t crop_size_byte = 0;
    uint32_t buf_offset = 0;
    void *blender_dev = Blender0();

    csk_dma2d_init_t dma2d_para_in = {
        .dma_ch = dma_2d_ch6,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    };

    csk_dma2d_init_t dma2d_para_out = {
        .dma_ch = dma_2d_ch7,
        .burst_len = dma2d_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = hs_none,
    };

    csk_dma_2d_image_cfg_t dma2d_img_cfg = {
        .img_input_format = csk_image_format_xrgb,
        .img_width = 160,
        .img_height = 120,
        .start_col = (10+1),
        .start_row = (10+1),
        .end_col = 90,
        .end_row = 90,
        .img_output_fromat_transfer = csk_image_format_transfer_xrgb_crop,
        .img_rgb888_format = csk_image_rgb888_format,
    };

    csk_gpdma_init_t blender_d2back_input = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_1spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2p,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_fix,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = d2back_hs_num3,
    };

    csk_gpdma_init_t blender_d2out_output = {
            .dma_ch = gp_dma_ch1,
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

    Blender_InitTypeDef blender_cfg = {
            .blender_mode = BLENDER_MODE_FILL,
            .alpha_mode = BLENDER_ALPHA_MODE_2,
            .back_format = BLENDER_BACK_FORMAT_RGB888,
            .fore_format = BLENDER_FORE_FORMAT_RGB565,
            .img_width = 80,
            .img_height = 80,
            .color = 0xFF0000,
            .alpha = 0x80,
            .burst_thd = 8,
    };


    rgb888_size_byte = dma2d_img_cfg.img_width * dma2d_img_cfg.img_height * RGB888_PIXEL_BYTE;   // RGB888
    crop_size_byte = (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1) * (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1) * RGB888_PIXEL_BYTE;   // RGB888
    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    buf_src = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf_src, error1);
    VIDEO_LOG("buf_src=0x%08x size=0x%x byte", buf_src, rgb888_size_byte);

    buf_dts = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf_dts, error1);
    VIDEO_LOG("buf_dts=0x%08x size=0x%x byte", buf_dts, rgb888_size_byte);

    buf_crop_src = tiny_malloc(crop_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf_crop_src, error1);
    VIDEO_LOG("buf_crop_src=0x%08x size=0x%x byte", buf_crop_src, crop_size_byte);

    buf_crop_dts = tiny_malloc(crop_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(buf_crop_dts, error1);
    VIDEO_LOG("buf_crop_dts=0x%08x size=0x%x byte", buf_crop_dts, crop_size_byte);

    rgb888_colorbar_create(buf_src, dma2d_img_cfg.img_width, dma2d_img_cfg.img_height, 20);
    rgb888_line_color(buf_dts, 0, dma2d_img_cfg.img_width, 0, dma2d_img_cfg.img_height, dma2d_img_cfg.img_width, RGB888_BRED);
    memset(buf_crop_src, 0, crop_size_byte);
    memset(buf_crop_dts, 0, crop_size_byte);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);


#if 1
    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&dma2d_para_in, gpdma_2d_blander_in_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(dma2d_para_in.dma_ch, &dma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << dma2d_para_in.dma_ch);
    copy_cnt = test_gpdma_2d_blander_in_finish_cnt;
    VIDEO_LOG("copy_cnt=%d", copy_cnt);

    ret = DMA2D_Start_Normal(dma2d_para_in.dma_ch, buf_src, buf_crop_src, rgb888_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(copy_cnt, test_gpdma_2d_blander_in_finish_cnt, timeout, error2);
    copy_cnt = test_gpdma_2d_blander_in_finish_cnt;
    VIDEO_LOG("copy_cnt=%d", copy_cnt);

    ret = DMA2D_Stop(dma2d_para_in.dma_ch);
    //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    //while(1);
#endif


    ret = GPDMA_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_d2back_input, gpdma_2d_blander_in_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_d2out_output, gpdma_2d_blander_out_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);


    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_BLENDER_CLK = 0x1; // blender clk enable
    IP_AP_CFG->REG_SW_RESET.bit.VIDEO_RESET = 0x1;

    /* blender init */
    ret = Blender_Initialize(blender_dev, &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    copy_cnt = test_gpdma_2d_blander_out_finish_cnt;

    ret = GPDMA_Start_Normal(blender_d2back_input.dma_ch, (uint32_t*)buf_crop_src, (uint32_t*)D2BACK_BUF, crop_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Start_Normal(blender_d2out_output.dma_ch, (uint32_t*)D2OUT_BUF, (uint32_t*)buf_crop_dts, crop_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    Blender_Start(blender_dev);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(copy_cnt, test_gpdma_2d_blander_out_finish_cnt, timeout, error2);
    copy_cnt = test_gpdma_2d_blander_in_finish_cnt;
    VIDEO_LOG("copy_cnt=%d", copy_cnt);

    ret = GPDMA_Stop(blender_d2back_input.dma_ch);
    //CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Stop(blender_d2out_output.dma_ch);
    //CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    Blender_Stop(blender_dev);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
   // while(1);



    ret = DMA2D_Config(&dma2d_para_out, gpdma_2d_blander_out_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(dma2d_para_out.dma_ch, &dma2d_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << dma2d_para_out.dma_ch);
    blender_out_cnt = test_gpdma_2d_blander_out_finish_cnt;
    VIDEO_LOG("blender_out_cnt=%d", blender_out_cnt);

//    VIDEO_LOG("ENC_IN2D=0x%x", IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS);
//    VIDEO_LOG("CTRL1=0x%x", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.all);
//    VIDEO_LOG("CTRL3=0x%x", IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.all);
//    VIDEO_LOG("IMAGE_FEATURE=0x%x", IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all);

    IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS = 0x0;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9 = ((dma2d_img_cfg.img_width - (dma2d_img_cfg.end_col - dma2d_img_cfg.start_col + 1)) * RGB888_PIXEL_BYTE) + 4;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 1;
    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = (dma2d_img_cfg.end_row - dma2d_img_cfg.start_row + 1);
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN9 = 0;
    IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN9 = 1;


    buf_offset = (dma2d_img_cfg.img_width * (dma2d_img_cfg.start_row - 1 - 20) + (dma2d_img_cfg.start_col - 1 - 20)) * RGB888_PIXEL_BYTE;
    VIDEO_LOG("buf_offset=%d", buf_offset);
    ret = DMA2D_Start_Normal(dma2d_para_out.dma_ch, buf_crop_dts, buf_dts + buf_offset, rgb888_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    timeout = 3000000;  // wait blender done, timeout=1000ms
    CHECK_EQ_TIMEOUT_EXIT(blender_out_cnt, test_gpdma_2d_blander_out_finish_cnt, timeout, error2);
    blender_out_cnt = test_gpdma_2d_blander_out_finish_cnt;
    VIDEO_LOG("blender_out_cnt=%d", blender_out_cnt);


    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    while(1);

    ret = SUCCESS;

error2:
error1:
    tiny_free(buf_src);
    tiny_free(buf_dts);
    tiny_free(buf_crop_src);
    tiny_free(buf_crop_dts);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


