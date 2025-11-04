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

#define TEST_PIPE_DVP_GPDMA_CH           gp_dma_ch1
#define TEST_PIPE_QSPI_OUT_GPDMA_CH      gp_dma_ch2
#define TEST_PIPE_D2BLENDER_BACK_DMA_CH  gp_dma_ch3
#define TEST_PIPE_D2BLENDER_FORE_DMA_CH  gp_dma_ch4
#define TEST_PIPE_D2BLENDER_MASK_DMA_CH  gp_dma_ch0
#define TEST_PIPE_D2BLENDER_OUT_DMA_CH   gp_dma_ch5
#define TEST_PIPE_2D_RGB888_GPDMA_CH     dma_2d_ch6
#define TEST_PIPE_2D_Y8_GPDMA_CH         dma_2d_ch7
#define TEST_PIPE_ROTATE1_GPDMA_CH       dma_2d_ch8
#define TEST_PIPE_ROTATE2_GPDMA_CH       dma_2d_ch9
#define TEST_PIPE_ZOOM_GPDMA_CH          dma_2d_ch8

#define TEST_PIPE_DVP_MCLK_HZ           24000000     // ARCS 2MHz
#define TEST_PIPE_QSPI_OUT_CLK_HZ       24000000     // ARCS 12MHz

#ifndef MEM_PSRAM_ADDR
#define MEM_PSRAM_ADDR          0x28000000
#endif

/*
 *  TEST_PIPE_01_01:  camera 320x240 RGB565 -> DVP -> qspi_out
 *  TEST_PIPE_01_02:  camera 320x240 RGB565 -> DVP -> blender image -> qspi_out
 *  TEST_PIPE_01_03:  camera 320x240 RGB565 -> DVP auto buf -> blender color -> qspi_out
 *  TEST_PIPE_01_04:  camera 320x240 YUV422 -> DVP PSRAM -> YUV422 to Y8 -> YUV422 to RGB888 -> blender mask+color -> qspi_out
 *  TEST_PIPE_01_05:  camera 320x240 YUV422 -> DVP PSRAM -> rotate 90 -> YUV422 to Y8 -> YUV422 to RGB888 -> blender mask+color -> qspi_out
 *  TEST_PIPE_01_06:  camera 320x240 YUV422 -> DVP PSRAM -> YUV422 to Y8 -> YUV422 to RGB888 -> crop + zoom -> blender mask+color -> qspi_out
 *  TEST_PIPE_01_07:  camera 1920x1080 YUV422 -> DVP PSRAM
 *  TEST_PIPE_01_08:  camera 1920x1080 YUV422 -> DVP -> yuv2rgb zoom 1/2 960x540 PSRAM -> crop 320x240 -> blender mask+color -> qspi_out
 *
 *  TEST_PIPE_01_07:  camera 320x240 YUV422 -> DVP auto buf -> rotate 90 -> jpeg encode
 *  TEST_PIPE_01_08:  jpeg decode -> YUV422 to RGB888 -> crop + zoom -> blender color -> RGB
 *  TEST_PIPE_02_01:  qspi_out ¨C> qspi_in
 *  TEST_PIPE_02_02:  RGB 8wire ¨C> DVP
 */

static int32_t test_pipe_01_01(void);
static int32_t test_pipe_01_02(void);
static int32_t test_pipe_01_03(void);
static int32_t test_pipe_01_04(void);
static int32_t test_pipe_01_05(void);
static int32_t test_pipe_01_06(void);
static int32_t test_pipe_01_07(void);
static int32_t test_pipe_01_08(void);

static int32_t test_pipe_qspi_out_spd2010_320x240_image_flush(uint8_t *pbuf, qspi_lcd_format_t format, uint8_t lane_num, bool dma_en);
static int32_t test_pipe_qspi_out_spd2010_320x240_color_flush(uint32_t color, qspi_lcd_format_t format, uint8_t lane_num, bool dma_en);

void test_pipe(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    //test_pipe_01_01();        // pass
    //test_pipe_01_02();        // pass
    //test_pipe_01_03();        // pass
    //test_pipe_01_04();        // pass
    //test_pipe_01_05();
    //test_pipe_01_06();       // pass
    //test_pipe_01_07();
    test_pipe_01_08();        // pass 72h

    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return;

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return;
}


static DVP_InitTypeDef dvp_cfg_rgb565_320x240 = {
        .FrameWidth = 640,
        .FrameHeight = 240,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_LUMINA_8BIT,      // DVP_INPUT_FORM_YUV422_Y0CBY1CR
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_RIGHT,
};

static camera_config_t camera_cfg_rgb565_320x240 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_PIPE_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};

static DVP_InitTypeDef dvp_cfg_yuv422_320x240 = {
        .FrameWidth = 320,
        .FrameHeight = 240,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,      // DVP_INPUT_FORM_YUV422_Y0CBY1CR
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_RIGHT,
};

static camera_config_t camera_cfg_yuv422_320x240 = {
    .sccb_i2c_port = DVP_I2C_INDEX,
    .xclk_freq_hz = TEST_PIPE_DVP_MCLK_HZ,
    .pixel_format = PIXFORMAT_YUV422,
    .frame_size = FRAMESIZE_QVGA,
    .colorbar = 0,
};

static qspi_lcd_config_t qspi_out_cfg = {
        .clk_hz = TEST_PIPE_QSPI_OUT_CLK_HZ,
        .txio = QSPI_LCD_TXIO_DMA,              // QSPI_LCD_TXIO_PIO / QSPI_LCD_TXIO_DMA
        .cp = QSPI_LCD_CPOL1_CPOH1,
        .is_msb = true,
        .lane_num = 4,
        .width = 412,
        .height = 412,
        .format = QSPI_LCD_FORMAT_RGB565,
};

static Blender_InitTypeDef blender_cfg = {
        .blender_mode = BLENDER_MODE_FILL,
        .alpha_mode = BLENDER_ALPHA_MODE_2,
        .back_format = BLENDER_BACK_FORMAT_RGB565,
        .fore_format = BLENDER_FORE_FORMAT_RGB565,
        .img_width = 320,
        .img_height = 240,
        .color = 0x808080,
        .alpha = 0x80,
        .burst_thd = 8,
};

/************* GPDMA **********************************************/
static csk_gpdma_init_t dvp_gpdma_cfg = {
        .dma_ch = TEST_PIPE_DVP_GPDMA_CH,
        .burst_len = gpdma_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_p2m,
        .src_inc_mode = inc_mode_fix,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .sample_unit = gpdma_sample_unit_word,
        .handshake = dvp_hs_num5,
};

static csk_gpdma_init_t qspi_out_gpdma_cfg = {
        .dma_ch = TEST_PIPE_QSPI_OUT_GPDMA_CH,
        .burst_len = gpdma_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2p,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .prio_lvl = prio_mode_vhigh,
        .sample_unit = gpdma_sample_unit_word,
        .handshake = qspi_hs_num0,
};

static csk_gpdma_init_t blender_back_gpdma_cfg = {
        .dma_ch = TEST_PIPE_D2BLENDER_BACK_DMA_CH,
        .burst_len = gpdma_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2p,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .prio_lvl = prio_mode_vhigh,
        .sample_unit = gpdma_sample_unit_word,
        .handshake = d2back_hs_num3,
};

static csk_gpdma_init_t blender_fore_gpdma_cfg = {
        .dma_ch = TEST_PIPE_D2BLENDER_FORE_DMA_CH,
        .burst_len = gpdma_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2p,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .prio_lvl = prio_mode_vhigh,
        .sample_unit = gpdma_sample_unit_word,
        .handshake = d2fore_hs_num4,
};

static csk_gpdma_init_t blender_mask_gpdma_cfg = {
        .dma_ch = TEST_PIPE_D2BLENDER_MASK_DMA_CH,
        .burst_len = gpdma_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2p,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .prio_lvl = prio_mode_vhigh,
        .sample_unit = gpdma_sample_unit_word,
        .handshake = d2mask_hs_num2,
};

static csk_gpdma_init_t blender_out_gpdma_cfg = {
        .dma_ch = TEST_PIPE_D2BLENDER_OUT_DMA_CH,
        .burst_len = gpdma_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_p2m,
        .src_inc_mode = inc_mode_fix,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .sample_unit = gpdma_sample_unit_word,
        .handshake = d2out_hs_num1,
};

/************* 2D **********************************************/
static csk_dma2d_init_t gpdma2d_yuv422_rgb888_cfg = {
    .dma_ch = TEST_PIPE_2D_RGB888_GPDMA_CH,
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

static csk_dma_2d_image_cfg_t gpdma2d_img_yuv422_rgb888_cfg = {
    .img_input_format = csk_image_format_yuv422,
    .img_width = 320,
    .img_height = 240,
    .img_output_fromat_transfer = csk_image_format_transfer_yuv422_xrgb,
    .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    .img_rgb888_format = csk_image_rgb888_format,
};

static csk_dma2d_init_t gpdma2d_yuv422_y8_cfg = {
    .dma_ch = TEST_PIPE_2D_Y8_GPDMA_CH,
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

static csk_dma_2d_image_cfg_t gpdma2d_img_yuv422_y8_cfg = {
    .img_input_format = csk_image_format_yuv422,
    .img_width = 320,
    .img_height = 240,
    .img_output_fromat_transfer = csk_image_format_transfer_yuv422_y8,
    .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    .img_rgb888_format = csk_image_rgb888_format,
};

static csk_dma2d_init_t gpdma2d_zoom_cfg = {
    .dma_ch = TEST_PIPE_ZOOM_GPDMA_CH,
    .burst_len = dma2d_burst_len_8spl,
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

static csk_dma_2d_image_cfg_t gpdma2d_img_zoom_cfg = {
    .img_input_format = csk_image_format_xrgb,
    .img_width = 320,
    .img_height = 240,
    .img_zoom_scale = csk_image_zoom_scale_1_to_2,
    .img_rgb888_format = csk_image_rgb888_format,
};

static csk_dma_2d_rotate_cfg_t gpdam2d_rotate_para_1= {
    .dma2d_init = {
        .dma_ch = TEST_PIPE_ROTATE1_GPDMA_CH,
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
        .img_width = 320,
        .img_height = 240,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_cw_90,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
        .image_yuv422_rotate_mode = csk_image_yuv422_clockwise_rotate,
    }
};

static csk_dma_2d_rotate_cfg_t gpdam2d_rotate_para_2= {
    .dma2d_init = {
        .dma_ch = TEST_PIPE_ROTATE2_GPDMA_CH,
        .burst_len = dma2d_burst_len_8spl,
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
        .img_width = 320,
        .img_height = 240,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    }
};


static volatile uint32_t test_dvp_gpdma_finish_cnt = 0;
static volatile uint32_t test_qspi_out_gpdma_finish_cnt = 0;
static volatile uint32_t test_blender_back_gpdma_finish_cnt = 0;
static volatile uint32_t test_blender_fore_gpdma_finish_cnt = 0;
static volatile uint32_t test_blender_mask_gpdma_finish_cnt = 0;
static volatile uint32_t test_blender_out_gpdma_finish_cnt = 0;
static volatile uint32_t test_gpdma_2d_finish_cnt = 0;
static volatile uint32_t test_gpdma_2d_rotate1_finish_cnt = 0;
static volatile uint32_t test_gpdma_2d_rotate2_finish_cnt = 0;

static void dvp_gpdma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_dvp_gpdma_finish_cnt++;
}

static void qspi_out_gpdma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_qspi_out_gpdma_finish_cnt++;
}

static void blender_back_gpdma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_blender_back_gpdma_finish_cnt++;
}

static void blender_fore_gpdma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_blender_fore_gpdma_finish_cnt++;
}

static void blender_mask_gpdma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_blender_mask_gpdma_finish_cnt++;
}

static void blender_out_gpdma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_blender_out_gpdma_finish_cnt++;
}

static void gpdma_2d_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_gpdma_2d_finish_cnt++;
}

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


static void dvp_qspi_out_reset(void)
{
    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_SW_RESET.bit.VIDEO_RESET = 0x1;

    IP_AP_CFG->REG_SW_RESET.bit.VIC_RESET = 0x1;
    DELAY_MS(1);
    mmio_write32(IMAGE_PROC_BASE + 0x04, 1);   // vic enable
    mmio_write32(IMAGE_PROC_BASE + 0x18, 1);   // 0:spi  1:dvp

    __HAL_CRM_QSPI1_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG1.bit.SEL_QSPI1_CLK = 0x1;
    IP_AP_CFG->REG_SW_RESET.bit.QSPI1_RESET = 0x1;
    DELAY_MS(1);
    IP_AP_CFG->REG_DMA_SEL.bit.DMA_SEL = 0x1;   // 1:qspi  0:rgb

    mmio_write32(IMAGE_PROC_BASE + 0x10, 1);   // display enable
    mmio_write32(IMAGE_PROC_BASE + 0x14, 1);   // qspi enable

    mmio_write32(IMAGE_PROC_BASE + 0xC, 1);   // d2bledner enable
}


/* camera: 320x240 RGB565 -> dvp -> qspi_out */
static int32_t test_pipe_01_01(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t image_cnt = 0;
    uint32_t display_cnt = 0;
    bool dma_en = false;

    uint32_t image_size_byte = 0;
    uint8_t *dvp_buf = NULL;
    uint8_t *qspi_out_buf = NULL;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    camera_reset();
    dvp_reset();
    qspi_lcd_reset();

    /* malloc */
    image_size_byte = dvp_cfg_rgb565_320x240.FrameWidth * dvp_cfg_rgb565_320x240.FrameHeight * 1;
    VIDEO_LOG("malloc size=0x%x byte, tiny_mem_perused=%d", image_size_byte, tiny_mem_perused());
    dvp_buf = tiny_malloc(image_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dvp_buf, error1);
    VIDEO_LOG("dvp_buf=0x%08x size=0x%x byte", dvp_buf, image_size_byte);

    VIDEO_LOG("malloc size=0x%x byte, tiny_mem_perused=%d", image_size_byte, tiny_mem_perused());
    qspi_out_buf = tiny_malloc(image_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(qspi_out_buf, error1);
    VIDEO_LOG("qspi_out_buf=0x%08x size=0x%x byte", qspi_out_buf, image_size_byte);

    /* gpdma init */
    ret = GPDMA_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    ret = GPDMA_Config(&dvp_gpdma_cfg, dvp_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    ret = GPDMA_Config(&qspi_out_gpdma_cfg, qspi_out_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* dvp init */
    ret = dvp_init(&dvp_cfg_rgb565_320x240, TEST_PIPE_DVP_MCLK_HZ);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* camera init colorbar*/
    camera_reset();
    camera_init(&camera_cfg_rgb565_320x240);
    DELAY_MS(100);

    lcd_qspi_out_pinmux();
    lcd_qspi_out_reset();
    lcd_qspi_out_bl_enable();

    /* qspi_out init */
    if(qspi_out_cfg.txio == QSPI_LCD_TXIO_DMA) {
        dma_en = true;
    }
    qspi_out_cfg.txio = QSPI_LCD_TXIO_PIO;
    ret = qspi_lcd_init(&qspi_out_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* LCD SPD2010 init */
    if(qspi_out_cfg.format == QSPI_LCD_FORMAT_RGB565) {
        lcd_init(LCD_FORMAT_RGB565);
    } else {
        lcd_init(LCD_FORMAT_RGB888);
    }

    /* lcd display clear */
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB565_RED, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB565_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB565_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    DELAY_MS(100);

    /* dvp start */
    ret = dvp_start();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    image_cnt = test_dvp_gpdma_finish_cnt;
    display_cnt = image_cnt;
    /* FIFO clear */
    mmio_write32(GPDMA_BASE + 0x1B4, (1 << TEST_PIPE_DVP_GPDMA_CH));
    ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, dvp_buf, image_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    while(1)
    {
        /* dvp get new frame */
        if (image_cnt != test_dvp_gpdma_finish_cnt)
        {
            image_cnt = test_dvp_gpdma_finish_cnt;
            display_cnt = image_cnt;
            ret = dvp_stop();
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            VIDEO_LOG("display_cnt=%d", display_cnt);

            ret = qspi_lcd_init(&qspi_out_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            memcpy(qspi_out_buf, dvp_buf, image_size_byte);
            ret = test_pipe_qspi_out_spd2010_320x240_image_flush(qspi_out_buf, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

            /* DVP get next frame */
            ret = dvp_start();
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, dvp_buf, image_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());
            VIDEO_LOG("dvp_buf=0x%08x size=0x%x byte", dvp_buf, image_size_byte);
        }
    }

    ret = SUCCESS;

error2:
    dvp_reg_dump();
    qspi_lcd_reg_dump();
    gpdma_reg_dump(TEST_PIPE_DVP_GPDMA_CH);
    gpdma_reg_dump(TEST_PIPE_QSPI_OUT_GPDMA_CH);
    ap_cfg_reg_dump();
    dvp_stop();
    dvp_deinit();
    VIDEO_LOG("dvp_buf=0x%08x size=0x%x byte", dvp_buf, image_size_byte);
    VIDEO_LOG("qspi_out_buf=0x%08x size=0x%x byte", qspi_out_buf, image_size_byte);

error1:
    tiny_free(dvp_buf);
    tiny_free(qspi_out_buf);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* camera: 320x240 RGB565 -> dvp -> blender image -> qspi_out */
static int32_t test_pipe_01_02(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    bool dma_en = false;
    uint8_t *image_buf = NULL;
    uint8_t *blender_buf = NULL;
    uint32_t size_byte = 0;
    uint32_t image_cnt = 0;
    uint32_t blender_cnt = 0;
    uint32_t display_cnt = 0;
    uint32_t i = 0;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    camera_reset();
    dvp_reset();
    qspi_lcd_reset();

    /* malloc */
    size_byte = dvp_cfg_rgb565_320x240.FrameWidth * dvp_cfg_rgb565_320x240.FrameHeight * 1;
    image_buf = tiny_malloc(size_byte);
    CHECK_POINT_NOT_NULL_EXIT(image_buf, error1);
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, size_byte);
    blender_buf = tiny_malloc(size_byte);
    CHECK_POINT_NOT_NULL_EXIT(blender_buf, error1);
    VIDEO_LOG("blender_buf=0x%08x size=0x%x byte", blender_buf, size_byte);

    rgb565_colorbar_create((uint16_t *)blender_buf, 320, 240, 240/5);

    /* gpdma init */
    ret = GPDMA_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = GPDMA_Config(&dvp_gpdma_cfg, dvp_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = GPDMA_Config(&qspi_out_gpdma_cfg, qspi_out_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = GPDMA_Config(&blender_back_gpdma_cfg, blender_back_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_fore_gpdma_cfg, blender_fore_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_out_gpdma_cfg, blender_out_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* blender init */
    ret = Blender_Initialize(Blender0(), &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* dvp init */
    ret = dvp_init(&dvp_cfg_rgb565_320x240, TEST_PIPE_DVP_MCLK_HZ);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    /* camera init colorbar*/
    camera_init(&camera_cfg_rgb565_320x240);
    DELAY_MS(100);

    lcd_qspi_out_pinmux();
    lcd_qspi_out_reset();
    lcd_qspi_out_bl_enable();

    /* qspi_out init */
    if(qspi_out_cfg.txio == QSPI_LCD_TXIO_DMA) {
        dma_en = true;
    }
    qspi_out_cfg.txio = QSPI_LCD_TXIO_PIO;
    ret = qspi_lcd_init(&qspi_out_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* LCD SPD2010 init */
    if(qspi_out_cfg.format == QSPI_LCD_FORMAT_RGB565) {
        lcd_init(LCD_FORMAT_RGB565);
    } else {
        lcd_init(LCD_FORMAT_RGB888);
    }

    /* lcd display clear */
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB565_RED, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB565_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB565_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    DELAY_MS(100);

    /* blender init */
    blender_cfg.color = 0xFF0000;   // B8:G8:R8
    blender_cfg.alpha = 0x40;
    blender_cfg.blender_mode = BLENDER_MODE_MAP;
    ret = Blender_Initialize(Blender0(), &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* dvp start */
    ret = dvp_start();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    image_cnt = test_dvp_gpdma_finish_cnt;
    display_cnt = 0;
    blender_cnt = 0;

    /* FIFO clear */
    mmio_write32(GPDMA_BASE + 0x1B4, (1 << TEST_PIPE_DVP_GPDMA_CH));
    ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, image_buf, size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    while(1)
    {
        /* dvp get new frame */
        if (image_cnt != test_dvp_gpdma_finish_cnt)
        {
            image_cnt = test_dvp_gpdma_finish_cnt;
            ret = dvp_stop();
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());

            swap_uint16((uint16_t *)image_buf, 320 * 240);

            blender_cnt = test_blender_out_gpdma_finish_cnt;
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_BACK_DMA_CH, (uint32_t*)image_buf, (uint32_t*)D2BACK_BUF, size_byte / sizeof(uint32_t));
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_FORE_DMA_CH, (uint32_t*)blender_buf, (uint32_t*)D2FORE_BUF, size_byte / sizeof(uint32_t));
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_OUT_DMA_CH, (uint32_t*)D2OUT_BUF, (uint32_t*)image_buf, size_byte / sizeof(uint32_t));
            ret = Blender_Start(Blender0());
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(blender_cnt, test_blender_out_gpdma_finish_cnt, timeout, error2);
            blender_cnt = test_blender_out_gpdma_finish_cnt;
            VIDEO_LOG("blender_cnt=%d", blender_cnt);

            rgb565_square_create((uint16_t *)image_buf, 320, 150, 100, 100);
            swap_uint16((uint16_t *)image_buf, 320 * 240);

            //ret = qspi_lcd_init(&qspi_out_cfg);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            ret = test_pipe_qspi_out_spd2010_320x240_image_flush(image_buf, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            VIDEO_LOG("display_cnt=%d", display_cnt++);

            /* DVP get next frame */
            ret = dvp_start();
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, image_buf, size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, size_byte);
        }
    }

    ret = SUCCESS;

error2:
    dvp_reg_dump();
    qspi_lcd_reg_dump();
    blender_reg_dump();
    gpdma_reg_dump(TEST_PIPE_DVP_GPDMA_CH);
    gpdma_reg_dump(TEST_PIPE_QSPI_OUT_GPDMA_CH);
    gpdma_reg_dump(TEST_PIPE_D2BLENDER_BACK_DMA_CH);
    gpdma_reg_dump(TEST_PIPE_D2BLENDER_OUT_DMA_CH);
    dvp_stop();
    dvp_deinit();
    Blender_Stop(Blender0());
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, size_byte);

error1:
    tiny_free(image_buf);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* camera: 320x240 RGB565 -> dvp auto buf -> blender color -> qspi_out */
static int32_t test_pipe_01_03(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    bool dma_en = false;
    uint8_t *image_buf1 = NULL;
    uint8_t *image_buf2 = NULL;
    uint8_t *display_buf = NULL;
    uint32_t size_byte = 0;
    uint32_t image_cnt = 0;
    uint32_t blender_cnt = 0;
    uint32_t display_cnt = 0;
    uint32_t i = 0;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    camera_reset();
    dvp_reset();
    qspi_lcd_reset();

    /* malloc */
    size_byte = dvp_cfg_rgb565_320x240.FrameWidth * dvp_cfg_rgb565_320x240.FrameHeight * 1;
    image_buf1 = tiny_malloc(size_byte);
    CHECK_POINT_NOT_NULL_EXIT(image_buf1, error1);
    VIDEO_LOG("image_buf1=0x%08x size=0x%x byte", image_buf1, size_byte);
    image_buf2 = tiny_malloc(size_byte);
    CHECK_POINT_NOT_NULL_EXIT(image_buf2, error1);
    VIDEO_LOG("image_buf2=0x%08x size=0x%x byte", image_buf2, size_byte);

    /* gpdma init */
    ret = GPDMA_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    dvp_gpdma_cfg.src_mode = address_mode_pipo;
    dvp_gpdma_cfg.dst_mode = address_mode_pipo;
    ret = GPDMA_Config(&dvp_gpdma_cfg, dvp_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = GPDMA_Config(&qspi_out_gpdma_cfg, qspi_out_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = GPDMA_Config(&blender_back_gpdma_cfg, blender_back_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_out_gpdma_cfg, blender_out_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* blender init */
    ret = Blender_Initialize(Blender0(), &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* dvp init */
    ret = dvp_init(&dvp_cfg_rgb565_320x240, TEST_PIPE_DVP_MCLK_HZ);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    /* camera init colorbar*/
    camera_init(&camera_cfg_rgb565_320x240);
    DELAY_MS(100);

    lcd_qspi_out_pinmux();
    lcd_qspi_out_reset();
    lcd_qspi_out_bl_enable();

    /* qspi_out init */
    if(qspi_out_cfg.txio == QSPI_LCD_TXIO_DMA) {
        dma_en = true;
    }
    qspi_out_cfg.txio = QSPI_LCD_TXIO_PIO;
    ret = qspi_lcd_init(&qspi_out_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* LCD SPD2010 init */
    if(qspi_out_cfg.format == QSPI_LCD_FORMAT_RGB565) {
        lcd_init(LCD_FORMAT_RGB565);
    } else {
        lcd_init(LCD_FORMAT_RGB888);
    }

    /* lcd display clear */
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB565_RED, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB565_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB565_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    DELAY_MS(100);

    /* blender init */
    blender_cfg.color = 0xFF0000;   // B8:G8:R8
    blender_cfg.alpha = 0x80;
    ret = Blender_Initialize(Blender0(), &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* dvp start */
    ret = dvp_start();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    image_cnt = test_dvp_gpdma_finish_cnt;
    display_cnt = 0;
    blender_cnt = 0;

    /* FIFO clear */
    mmio_write32(GPDMA_BASE + 0x1B4, (1 << TEST_PIPE_DVP_GPDMA_CH));
    /* PO block len */
    mmio_write32(GPDMA_BASE + 0x274 + 0x04 * TEST_PIPE_DVP_GPDMA_CH, size_byte / sizeof(uint32_t));
    //ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, image_buf, size_byte / sizeof(uint32_t));
    ret = GPDMA_Start_PiPo(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, (void*)VIC_BUF, image_buf1, image_buf2, size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    while(1)
    {
        /* dvp get new frame */
        if (image_cnt != test_dvp_gpdma_finish_cnt)
        {
            image_cnt = test_dvp_gpdma_finish_cnt;
            VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());
            if(image_cnt & 0x1) {
                VIDEO_LOG("image_buf1=0x%08x size=0x%x byte", image_buf1, size_byte);
                display_buf = image_buf1;
            } else {
                VIDEO_LOG("image_buf2=0x%08x size=0x%x byte", image_buf2, size_byte);
                display_buf = image_buf2;
            }

            swap_uint16((uint16_t *)display_buf, 320 * 240);

            blender_cnt = test_blender_out_gpdma_finish_cnt;
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_BACK_DMA_CH, (uint32_t*)display_buf, (uint32_t*)D2BACK_BUF, size_byte / sizeof(uint32_t));
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_OUT_DMA_CH, (uint32_t*)D2OUT_BUF, (uint32_t*)display_buf, size_byte / sizeof(uint32_t));
            ret = Blender_Start(Blender0());
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(blender_cnt, test_blender_out_gpdma_finish_cnt, timeout, error2);
            blender_cnt = test_blender_out_gpdma_finish_cnt;
            VIDEO_LOG("blender_cnt=%d", blender_cnt);

            rgb565_square_create((uint16_t *)display_buf, 320, 150, 100, 100);
            swap_uint16((uint16_t *)display_buf, 320 * 240);

            ret = qspi_lcd_init(&qspi_out_cfg);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            ret = test_pipe_qspi_out_spd2010_320x240_image_flush(display_buf, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            VIDEO_LOG("display_cnt=%d", display_cnt++);
        }

//        ret = qspi_lcd_init(&qspi_out_cfg);
//        ret = test_pipe_qspi_out_spd2010_320x240_image_flush(image_buf1, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
//        VIDEO_LOG("display_cnt=%d", display_cnt++);
    }

    ret = SUCCESS;

error2:
    dvp_reg_dump();
    qspi_lcd_reg_dump();
    blender_reg_dump();
    gpdma_reg_dump(TEST_PIPE_DVP_GPDMA_CH);
    gpdma_reg_dump(TEST_PIPE_QSPI_OUT_GPDMA_CH);
    gpdma_reg_dump(TEST_PIPE_D2BLENDER_BACK_DMA_CH);
    gpdma_reg_dump(TEST_PIPE_D2BLENDER_OUT_DMA_CH);
    dvp_stop();
    dvp_deinit();
    Blender_Stop(Blender0());
    VIDEO_LOG("image_buf1=0x%08x size=0x%x byte", image_buf1, size_byte);
    VIDEO_LOG("image_buf2=0x%08x size=0x%x byte", image_buf2, size_byte);

error1:
    tiny_free(image_buf1);
    tiny_free(image_buf2);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* camera 320x240 YUV422 -> DVP auto buf PSRAM -> rotate 90 -> YUV422 to Y8 -> YUV422 to RGB888 -> crop + zoom -> blender mask+color -> qspi_out */
static int32_t test_pipe_01_04(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    bool dma_en = false;
    bool dvp_pipo_en = false;
    uint8_t *dvp_buf1 = NULL;
    uint8_t *dvp_buf2 = NULL;
    uint8_t *yuv422_buf = NULL;
    uint8_t *y8_buf = NULL;
    uint8_t *rgb888_buf = NULL;
    uint8_t *blender_mask_buf = NULL;
    uint8_t *display_buf = NULL;
    uint32_t pixel_size_byte = 0;
    uint32_t yuv422_size_byte = 0;
    uint32_t rgb888_size_byte = 0;
    uint32_t image_cnt = 0;
    uint32_t gpdma2d_cnt = 0;
    uint32_t blender_cnt = 0;
    uint32_t display_cnt = 0;
    uint32_t i = 0;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    camera_reset();
    dvp_reset();
    qspi_lcd_reset();

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    pixel_size_byte = dvp_cfg_yuv422_320x240.FrameWidth * dvp_cfg_yuv422_320x240.FrameHeight;
    yuv422_size_byte = dvp_cfg_yuv422_320x240.FrameWidth * dvp_cfg_yuv422_320x240.FrameHeight * 2;
    rgb888_size_byte = dvp_cfg_yuv422_320x240.FrameWidth * dvp_cfg_yuv422_320x240.FrameHeight * 3;

    dvp_buf1 = tiny_malloc(yuv422_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dvp_buf1, error1);
    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, yuv422_size_byte);

    dvp_buf2 = tiny_malloc(yuv422_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dvp_buf2, error1);
    VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, yuv422_size_byte);

    y8_buf = tiny_malloc(pixel_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(y8_buf, error1);
    VIDEO_LOG("y8_buf=0x%08x size=0x%x byte", y8_buf, pixel_size_byte);

    rgb888_buf = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(rgb888_buf, error1);
    VIDEO_LOG("rgb888_buf=0x%08x size=0x%x byte", rgb888_buf, rgb888_size_byte);

    blender_mask_buf = tiny_malloc(pixel_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(blender_mask_buf, error1);
    VIDEO_LOG("blender_mask_buf=0x%08x size=0x%x byte", blender_mask_buf, pixel_size_byte);

    memset(blender_mask_buf, 0, pixel_size_byte);
    raw8_line_color(blender_mask_buf, 150, 250, 100, 100, 320, 0xFF);
    raw8_line_color(blender_mask_buf, 150, 250, 200, 200, 320, 0xFF);
    raw8_line_color(blender_mask_buf, 150, 150, 100, 200, 320, 0xFF);
    raw8_line_color(blender_mask_buf, 250, 250, 100, 200, 320, 0xFF);

    /* gpdma init */
    ret = GPDMA_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    if(true == dvp_pipo_en)
    {
        dvp_gpdma_cfg.src_mode = address_mode_pipo;
        dvp_gpdma_cfg.dst_mode = address_mode_pipo;
    }
    ret = GPDMA_Config(&dvp_gpdma_cfg, dvp_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = GPDMA_Config(&qspi_out_gpdma_cfg, qspi_out_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&gpdma2d_yuv422_rgb888_cfg, gpdma_2d_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_rgb888_cfg.dma_ch, &gpdma2d_img_yuv422_rgb888_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Config(&gpdma2d_yuv422_y8_cfg, gpdma_2d_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_y8_cfg.dma_ch, &gpdma2d_img_yuv422_y8_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = GPDMA_Config(&blender_back_gpdma_cfg, blender_back_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_mask_gpdma_cfg, blender_mask_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_out_gpdma_cfg, blender_out_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* dvp init */
    ret = dvp_init(&dvp_cfg_yuv422_320x240, TEST_PIPE_DVP_MCLK_HZ);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* camera init colorbar*/
    camera_init(&camera_cfg_yuv422_320x240);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    lcd_qspi_out_pinmux();
    lcd_qspi_out_reset();
    lcd_qspi_out_bl_enable();

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* qspi_out init */
    qspi_out_cfg.format = QSPI_LCD_FORMAT_RGB888;
    if(qspi_out_cfg.txio == QSPI_LCD_TXIO_DMA) {
        dma_en = true;
    }
    qspi_out_cfg.txio = QSPI_LCD_TXIO_PIO;
    ret = qspi_lcd_init(&qspi_out_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* LCD SPD2010 init */
    if(qspi_out_cfg.format == QSPI_LCD_FORMAT_RGB565) {
        lcd_init(LCD_FORMAT_RGB565);
    } else {
        lcd_init(LCD_FORMAT_RGB888);
    }
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* lcd display clear */
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB888_RED, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);            // Error after deletion
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB888_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB888_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* blender init */
    blender_cfg.color = 0xFF0000;   // B8:G8:R8
    blender_cfg.alpha = 0x80;
    blender_cfg.back_format = BLENDER_BACK_FORMAT_RGB888;
    blender_cfg.blender_mode = BLENDER_MODE_FILL;
    blender_cfg.alpha_mode = BLENDER_ALPHA_MODE_1;
    ret = Blender_Initialize(Blender0(), &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* dvp start */
    ret = dvp_start();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    image_cnt = test_dvp_gpdma_finish_cnt;
    display_cnt = 0;
    blender_cnt = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    mmio_write32(GPDMA_BASE + 0x1B4, (1 << TEST_PIPE_DVP_GPDMA_CH));

    /* DVP GPDMA start */
    if(true == dvp_pipo_en)
    {
        /* PO block len */
        mmio_write32(GPDMA_BASE + 0x274 + 0x04 * TEST_PIPE_DVP_GPDMA_CH, yuv422_size_byte / sizeof(uint32_t));
        ret = GPDMA_Start_PiPo(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, (void*)VIC_BUF, dvp_buf1, dvp_buf2, yuv422_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    }
    else
    {
        ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, dvp_buf1, yuv422_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    }

    while(1)
    {
        /* dvp get new frame */
        if (image_cnt != test_dvp_gpdma_finish_cnt)
        {
            /* dvp buf */
            if(true == dvp_pipo_en)
            {
                image_cnt = test_dvp_gpdma_finish_cnt;
                VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());

                if(image_cnt & 0x1) {
                    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, yuv422_size_byte);
                    yuv422_buf = dvp_buf1;
                } else {
                    VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, yuv422_size_byte);
                    yuv422_buf = dvp_buf2;
                }
            }
            else
            {
                ret = dvp_stop();
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

                image_cnt = test_dvp_gpdma_finish_cnt;
                VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());
                yuv422_buf = dvp_buf1;
                VIDEO_LOG("yuv422_buf=0x%08x size=0x%x byte", yuv422_buf, yuv422_size_byte);
            }


            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_rgb888_cfg.dma_ch));

            /* 2D: YUV422 to RGB888 */
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            ret = DMA2D_Start_Normal(gpdma2d_yuv422_rgb888_cfg.dma_ch, yuv422_buf, rgb888_buf, yuv422_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(gpdma2d_cnt, test_gpdma_2d_finish_cnt, timeout, error2);
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            VIDEO_LOG("gpdma2d_cnt=%d", gpdma2d_cnt);
            VIDEO_LOG("rgb888_buf=0x%08x size=0x%x byte", rgb888_buf, rgb888_size_byte);

            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_rgb888_cfg.dma_ch));

            /* 2D: YUV422 to RGB888 config */
            ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_rgb888_cfg.dma_ch, &gpdma2d_img_yuv422_rgb888_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);


            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_y8_cfg.dma_ch));

            /* 2D: YUV422 to Y8 */
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            ret = DMA2D_Start_Normal(gpdma2d_yuv422_y8_cfg.dma_ch, yuv422_buf, y8_buf, yuv422_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(gpdma2d_cnt, test_gpdma_2d_finish_cnt, timeout, error2);
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            VIDEO_LOG("gpdma2d_cnt=%d", gpdma2d_cnt);
            VIDEO_LOG("y8_buf=0x%08x size=0x%x byte", y8_buf, pixel_size_byte);

            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_y8_cfg.dma_ch));

            /* 2D: YUV422 to Y8 config */
            ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_y8_cfg.dma_ch, &gpdma2d_img_yuv422_y8_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);


            /* blender */
            blender_cnt = test_blender_out_gpdma_finish_cnt;
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_BACK_DMA_CH, (uint32_t*)rgb888_buf, (uint32_t*)D2BACK_BUF, rgb888_size_byte / sizeof(uint32_t));
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_MASK_DMA_CH, (uint32_t*)blender_mask_buf, (uint32_t*)D2MASK_BUF, pixel_size_byte / sizeof(uint32_t));
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_OUT_DMA_CH, (uint32_t*)D2OUT_BUF, (uint32_t*)rgb888_buf, rgb888_size_byte / sizeof(uint32_t));
            ret = Blender_Start(Blender0());
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(blender_cnt, test_blender_out_gpdma_finish_cnt, timeout, error2);
            blender_cnt = test_blender_out_gpdma_finish_cnt;
            VIDEO_LOG("blender_cnt=%d", blender_cnt);

            /* qspi out */
            display_buf = rgb888_buf;
            VIDEO_LOG("display_buf=0x%08x size=0x%x byte", display_buf, rgb888_size_byte);
            //ret = qspi_lcd_init(&qspi_out_cfg);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            ret = test_pipe_qspi_out_spd2010_320x240_image_flush(display_buf, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            VIDEO_LOG("display_cnt=%d", display_cnt++);

            /* dvp start */
            if(true != dvp_pipo_en)
            {
                ret = dvp_start();
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
                mmio_write32(GPDMA_BASE + 0x1B4, (1 << TEST_PIPE_DVP_GPDMA_CH));
                ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, dvp_buf1, yuv422_size_byte / sizeof(uint32_t));
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            }
        }
    }

    ret = SUCCESS;

error2:
    dvp_reg_dump();
    qspi_lcd_reg_dump();
    blender_reg_dump();
    gpdma_reg_dump(TEST_PIPE_DVP_GPDMA_CH);
    gpdma_reg_dump(TEST_PIPE_QSPI_OUT_GPDMA_CH);
    gpdma_reg_dump(TEST_PIPE_D2BLENDER_BACK_DMA_CH);
    gpdma_reg_dump(TEST_PIPE_D2BLENDER_OUT_DMA_CH);
    dvp_stop();
    dvp_deinit();
    Blender_Stop(Blender0());
    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, yuv422_size_byte);
    VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, yuv422_size_byte);

error1:
    tiny_free(dvp_buf1);
    tiny_free(dvp_buf2);
    tiny_free(y8_buf);
    tiny_free(rgb888_buf);
    tiny_free(blender_mask_buf);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* camera 320x240 YUV422 -> DVP auto buf PSRAM -> rotate 90 -> YUV422 to Y8 -> YUV422 to RGB888 -> crop + zoom -> blender mask+color -> qspi_out */
static int32_t test_pipe_01_05(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    bool dma_en = false;
    bool dvp_pipo_en = false;
    uint8_t *dvp_buf1 = NULL;
    uint8_t *dvp_buf2 = NULL;
    uint8_t *yuv422_buf = NULL;
    uint8_t *yuv422_rotate_buf = NULL;
    uint8_t *yuv422_8line_buf1 = NULL;
    uint8_t *yuv422_8line_buf2 = NULL;
    uint8_t *y8_buf = NULL;
    uint8_t *rgb888_buf = NULL;
    uint8_t *blender_mask_buf = NULL;
    uint8_t *display_buf = NULL;
    uint32_t pixel_size_byte = 0;
    uint32_t yuv422_8line_byte = 0;
    uint32_t yuv422_size_byte = 0;
    uint32_t rgb888_size_byte = 0;
    uint32_t image_cnt = 0;
    uint32_t rotate1_cnt = 0;
    uint32_t rotate2_cnt = 0;
    uint32_t gpdma2d_cnt = 0;
    uint32_t blender_cnt = 0;
    uint32_t display_cnt = 0;
    uint32_t i = 0;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    camera_reset();
    dvp_reset();
    qspi_lcd_reset();

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* malloc */
    pixel_size_byte = dvp_cfg_yuv422_320x240.FrameWidth * dvp_cfg_yuv422_320x240.FrameHeight;
    yuv422_8line_byte = dvp_cfg_yuv422_320x240.FrameWidth * 8 * 2;
    yuv422_size_byte = dvp_cfg_yuv422_320x240.FrameWidth * dvp_cfg_yuv422_320x240.FrameHeight * 2;
    rgb888_size_byte = dvp_cfg_yuv422_320x240.FrameWidth * dvp_cfg_yuv422_320x240.FrameHeight * 3;

    dvp_buf1 = tiny_malloc(yuv422_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dvp_buf1, error1);
    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, yuv422_size_byte);

    dvp_buf2 = tiny_malloc(yuv422_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dvp_buf2, error1);
    VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, yuv422_size_byte);

    yuv422_8line_buf1 = tiny_malloc(yuv422_8line_byte);
    CHECK_POINT_NOT_NULL_EXIT(yuv422_8line_buf1, error1);
    VIDEO_LOG("yuv422_8line_buf1=0x%08x size=0x%x byte", yuv422_8line_buf1, yuv422_8line_byte);

    yuv422_8line_buf2 = tiny_malloc(yuv422_8line_byte);
    CHECK_POINT_NOT_NULL_EXIT(yuv422_8line_buf2, error1);
    VIDEO_LOG("yuv422_8line_buf2=0x%08x size=0x%x byte", yuv422_8line_buf2, yuv422_8line_byte);

    yuv422_rotate_buf = tiny_malloc(yuv422_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(yuv422_rotate_buf, error1);
    VIDEO_LOG("yuv422_rotate_buf=0x%08x size=0x%x byte", yuv422_rotate_buf, yuv422_size_byte);

    y8_buf = tiny_malloc(pixel_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(y8_buf, error1);
    VIDEO_LOG("y8_buf=0x%08x size=0x%x byte", y8_buf, pixel_size_byte);

    rgb888_buf = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(rgb888_buf, error1);
    VIDEO_LOG("rgb888_buf=0x%08x size=0x%x byte", rgb888_buf, rgb888_size_byte);

    blender_mask_buf = tiny_malloc(pixel_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(blender_mask_buf, error1);
    VIDEO_LOG("blender_mask_buf=0x%08x size=0x%x byte", blender_mask_buf, pixel_size_byte);

    memset(blender_mask_buf, 0, pixel_size_byte);
    raw8_line_color(blender_mask_buf, 150, 250, 100, 100, 320, 0xFF);
    raw8_line_color(blender_mask_buf, 150, 250, 200, 200, 320, 0xFF);
    raw8_line_color(blender_mask_buf, 150, 150, 100, 200, 320, 0xFF);
    raw8_line_color(blender_mask_buf, 250, 250, 100, 200, 320, 0xFF);

    /* gpdma init */
    ret = GPDMA_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    if(true == dvp_pipo_en)
    {
        dvp_gpdma_cfg.src_mode = address_mode_pipo;
        dvp_gpdma_cfg.dst_mode = address_mode_pipo;
    }
    ret = GPDMA_Config(&dvp_gpdma_cfg, dvp_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = GPDMA_Config(&qspi_out_gpdma_cfg, qspi_out_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&gpdma2d_yuv422_rgb888_cfg, gpdma_2d_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_rgb888_cfg.dma_ch, &gpdma2d_img_yuv422_rgb888_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Config(&gpdma2d_yuv422_y8_cfg, gpdma_2d_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_y8_cfg.dma_ch, &gpdma2d_img_yuv422_y8_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Rotate_Config(&gpdam2d_rotate_para_1, &gpdam2d_rotate_para_2, gpdma_2d_rotate1_callback, gpdma_2d_rotate2_callback, NULL, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Rotate_Config_Extend(&gpdam2d_rotate_para_1, &gpdam2d_rotate_para_2);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = GPDMA_Config(&blender_back_gpdma_cfg, blender_back_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_mask_gpdma_cfg, blender_mask_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_out_gpdma_cfg, blender_out_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* dvp init */
    ret = dvp_init(&dvp_cfg_yuv422_320x240, TEST_PIPE_DVP_MCLK_HZ);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* camera init colorbar*/
    camera_init(&camera_cfg_yuv422_320x240);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    lcd_qspi_out_pinmux();
    lcd_qspi_out_reset();
    lcd_qspi_out_bl_enable();

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* qspi_out init */
    qspi_out_cfg.format = QSPI_LCD_FORMAT_RGB888;
    if(qspi_out_cfg.txio == QSPI_LCD_TXIO_DMA) {
        dma_en = true;
    }
    qspi_out_cfg.txio = QSPI_LCD_TXIO_PIO;
    ret = qspi_lcd_init(&qspi_out_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* LCD SPD2010 init */
    if(qspi_out_cfg.format == QSPI_LCD_FORMAT_RGB565) {
        lcd_init(LCD_FORMAT_RGB565);
    } else {
        lcd_init(LCD_FORMAT_RGB888);
    }
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* lcd display clear */
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB888_RED, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);            // Error after deletion
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB888_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB888_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* blender init */
    blender_cfg.color = 0xFF0000;   // B8:G8:R8
    blender_cfg.alpha = 0x80;
    blender_cfg.back_format = BLENDER_BACK_FORMAT_RGB888;
    blender_cfg.blender_mode = BLENDER_MODE_FILL;
    blender_cfg.alpha_mode = BLENDER_ALPHA_MODE_1;
    ret = Blender_Initialize(Blender0(), &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* dvp start */
    ret = dvp_start();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    image_cnt = test_dvp_gpdma_finish_cnt;
    display_cnt = 0;
    blender_cnt = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    mmio_write32(GPDMA_BASE + 0x1B4, (1 << TEST_PIPE_DVP_GPDMA_CH));

    /* DVP GPDMA start */
    if(true == dvp_pipo_en)
    {
        /* PO block len */
        mmio_write32(GPDMA_BASE + 0x274 + 0x04 * TEST_PIPE_DVP_GPDMA_CH, yuv422_size_byte / sizeof(uint32_t));
        ret = GPDMA_Start_PiPo(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, (void*)VIC_BUF, dvp_buf1, dvp_buf2, yuv422_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    }
    else
    {
        ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, dvp_buf1, yuv422_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    }

    while(1)
    {
        /* dvp get new frame */
        if (image_cnt != test_dvp_gpdma_finish_cnt)
        {
            /* dvp buf */
            if(true == dvp_pipo_en)
            {
                image_cnt = test_dvp_gpdma_finish_cnt;
                VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());

                if(image_cnt & 0x1) {
                    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, yuv422_size_byte);
                    yuv422_buf = dvp_buf1;
                } else {
                    VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, yuv422_size_byte);
                    yuv422_buf = dvp_buf2;
                }
            }
            else
            {
                ret = dvp_stop();
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

                image_cnt = test_dvp_gpdma_finish_cnt;
                VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());
                yuv422_buf = dvp_buf1;
                VIDEO_LOG("yuv422_buf=0x%08x size=0x%x byte", yuv422_buf, yuv422_size_byte);
            }

#if 0
            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdam2d_rotate_para_1.dma2d_init.dma_ch));
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdam2d_rotate_para_2.dma2d_init.dma_ch));

            /* 2D: rotate 90 */
            rotate1_cnt = test_gpdma_2d_rotate1_finish_cnt;
            rotate2_cnt = test_gpdma_2d_rotate2_finish_cnt;
            ret = DMA2D_Start_Rotate(&gpdam2d_rotate_para_1, &gpdam2d_rotate_para_2, yuv422_buf, yuv422_8line_buf1, yuv422_8line_buf2, yuv422_rotate_buf, yuv422_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(rotate1_cnt, test_gpdma_2d_rotate1_finish_cnt, timeout, error2);
            timeout = 3000000;
            CHECK_EQ_TIMEOUT_EXIT(rotate2_cnt, test_gpdma_2d_rotate2_finish_cnt, timeout, error2);
            rotate1_cnt = test_gpdma_2d_rotate1_finish_cnt;
            rotate2_cnt = test_gpdma_2d_rotate2_finish_cnt;
            VIDEO_LOG("rotate1_cnt=%d", rotate1_cnt);
            VIDEO_LOG("rotate2_cnt=%d", rotate2_cnt);

            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdam2d_rotate_para_1.dma2d_init.dma_ch));
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdam2d_rotate_para_2.dma2d_init.dma_ch));

            /* 2D: rotate 90 config */
            ret = DMA2D_Image_Rotate_Config_Extend(&gpdam2d_rotate_para_1, &gpdam2d_rotate_para_2);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);


            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdam2d_rotate_para_1.dma2d_init.dma_ch));
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdam2d_rotate_para_2.dma2d_init.dma_ch));

            /* 2D: rotate 90 */
            rotate1_cnt = test_gpdma_2d_rotate1_finish_cnt;
            rotate2_cnt = test_gpdma_2d_rotate2_finish_cnt;
            ret = DMA2D_Start_Rotate(&gpdam2d_rotate_para_1, &gpdam2d_rotate_para_2, yuv422_rotate_buf, yuv422_8line_buf1, yuv422_8line_buf2, yuv422_buf, yuv422_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(rotate1_cnt, test_gpdma_2d_rotate1_finish_cnt, timeout, error2);
            timeout = 3000000;
            CHECK_EQ_TIMEOUT_EXIT(rotate2_cnt, test_gpdma_2d_rotate2_finish_cnt, timeout, error2);
            rotate1_cnt = test_gpdma_2d_rotate1_finish_cnt;
            rotate2_cnt = test_gpdma_2d_rotate2_finish_cnt;
            VIDEO_LOG("rotate1_cnt=%d", rotate1_cnt);
            VIDEO_LOG("rotate2_cnt=%d", rotate2_cnt);

            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdam2d_rotate_para_1.dma2d_init.dma_ch));
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdam2d_rotate_para_2.dma2d_init.dma_ch));

            /* 2D: rotate 90 config */
            ret = DMA2D_Image_Rotate_Config_Extend(&gpdam2d_rotate_para_1, &gpdam2d_rotate_para_2);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
#endif

            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_rgb888_cfg.dma_ch));

            /* 2D: YUV422 to RGB888 */
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            ret = DMA2D_Start_Normal(gpdma2d_yuv422_rgb888_cfg.dma_ch, yuv422_buf, rgb888_buf, yuv422_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(gpdma2d_cnt, test_gpdma_2d_finish_cnt, timeout, error2);
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            VIDEO_LOG("gpdma2d_cnt=%d", gpdma2d_cnt);
            VIDEO_LOG("rgb888_buf=0x%08x size=0x%x byte", rgb888_buf, rgb888_size_byte);

            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_rgb888_cfg.dma_ch));

            /* 2D: YUV422 to RGB888 config */
            ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_rgb888_cfg.dma_ch, &gpdma2d_img_yuv422_rgb888_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);


            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_y8_cfg.dma_ch));

            /* 2D: YUV422 to Y8 */
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            ret = DMA2D_Start_Normal(gpdma2d_yuv422_y8_cfg.dma_ch, yuv422_buf, y8_buf, yuv422_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(gpdma2d_cnt, test_gpdma_2d_finish_cnt, timeout, error2);
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            VIDEO_LOG("gpdma2d_cnt=%d", gpdma2d_cnt);
            VIDEO_LOG("y8_buf=0x%08x size=0x%x byte", y8_buf, pixel_size_byte);

            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_y8_cfg.dma_ch));

            /* 2D: YUV422 to Y8 config */
            ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_y8_cfg.dma_ch, &gpdma2d_img_yuv422_y8_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);


            /* blender */
            blender_cnt = test_blender_out_gpdma_finish_cnt;
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_BACK_DMA_CH, (uint32_t*)rgb888_buf, (uint32_t*)D2BACK_BUF, rgb888_size_byte / sizeof(uint32_t));
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_MASK_DMA_CH, (uint32_t*)blender_mask_buf, (uint32_t*)D2MASK_BUF, pixel_size_byte / sizeof(uint32_t));
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_OUT_DMA_CH, (uint32_t*)D2OUT_BUF, (uint32_t*)rgb888_buf, rgb888_size_byte / sizeof(uint32_t));
            ret = Blender_Start(Blender0());
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(blender_cnt, test_blender_out_gpdma_finish_cnt, timeout, error2);
            blender_cnt = test_blender_out_gpdma_finish_cnt;
            VIDEO_LOG("blender_cnt=%d", blender_cnt);

            /* qspi out */
            display_buf = rgb888_buf;
            VIDEO_LOG("display_buf=0x%08x size=0x%x byte", display_buf, rgb888_size_byte);
            //ret = qspi_lcd_init(&qspi_out_cfg);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            ret = test_pipe_qspi_out_spd2010_320x240_image_flush(display_buf, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            VIDEO_LOG("display_cnt=%d", display_cnt++);

            /* dvp start */
            if(true != dvp_pipo_en)
            {
                ret = dvp_start();
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
                mmio_write32(GPDMA_BASE + 0x1B4, (1 << TEST_PIPE_DVP_GPDMA_CH));
                ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, dvp_buf1, yuv422_size_byte / sizeof(uint32_t));
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            }
        }
    }

    ret = SUCCESS;

error2:
    dvp_reg_dump();
    qspi_lcd_reg_dump();
    blender_reg_dump();
    gpdma_reg_dump(TEST_PIPE_DVP_GPDMA_CH);
    gpdma_reg_dump(TEST_PIPE_QSPI_OUT_GPDMA_CH);
    gpdma_reg_dump(TEST_PIPE_D2BLENDER_BACK_DMA_CH);
    gpdma_reg_dump(TEST_PIPE_D2BLENDER_OUT_DMA_CH);
    dvp_stop();
    dvp_deinit();
    Blender_Stop(Blender0());
    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, yuv422_size_byte);
    VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, yuv422_size_byte);

error1:
    tiny_free(dvp_buf1);
    tiny_free(dvp_buf2);
    tiny_free(yuv422_8line_buf1);
    tiny_free(yuv422_8line_buf2);
    tiny_free(yuv422_rotate_buf);
    tiny_free(y8_buf);
    tiny_free(rgb888_buf);
    tiny_free(blender_mask_buf);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* camera 640x480 YUV422 -> DVP PSRAM -> YUV422 to Y8 -> YUV422 to RGB888 -> zoom -> blender mask+color -> qspi_out */
static int32_t test_pipe_01_06(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    bool dma_en = false;
    bool dvp_pipo_en = false;
    uint8_t *dvp_buf1 = NULL;
    uint8_t *dvp_buf2 = NULL;
    uint8_t *yuv422_buf = NULL;
    uint8_t *y8_buf = NULL;
    uint8_t *rgb888_buf = NULL;
    uint8_t *blender_mask_buf = NULL;
    uint8_t *display_buf = NULL;
    uint32_t pixel_size = 0;
    uint32_t yuv422_size_byte = 0;
    uint32_t rgb888_size_byte = 0;
    uint32_t display_size = 0;
    uint32_t display_size_byte = 0;
    uint32_t image_cnt = 0;
    uint32_t gpdma2d_cnt = 0;
    uint32_t blender_cnt = 0;
    uint32_t display_cnt = 0;
    uint32_t i = 0;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    camera_cfg_yuv422_320x240.frame_size = FRAMESIZE_VGA;
    dvp_cfg_yuv422_320x240.FrameWidth = 640;
    dvp_cfg_yuv422_320x240.FrameHeight = 480;
    gpdma2d_img_yuv422_rgb888_cfg.img_width = 640;
    gpdma2d_img_yuv422_rgb888_cfg.img_height = 480;
    gpdma2d_img_yuv422_y8_cfg.img_width = 640;
    gpdma2d_img_yuv422_y8_cfg.img_height = 480;
    gpdma2d_img_zoom_cfg.img_width = 640;
    gpdma2d_img_zoom_cfg.img_height = 480;

    camera_reset();
    dvp_reset();
    qspi_lcd_reset();

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    pixel_size = dvp_cfg_yuv422_320x240.FrameWidth * dvp_cfg_yuv422_320x240.FrameHeight;
    yuv422_size_byte = dvp_cfg_yuv422_320x240.FrameWidth * dvp_cfg_yuv422_320x240.FrameHeight * 2;
    rgb888_size_byte = dvp_cfg_yuv422_320x240.FrameWidth * dvp_cfg_yuv422_320x240.FrameHeight * 3;

    /* zoom 1/2 */
    display_size = dvp_cfg_yuv422_320x240.FrameWidth / 2 * dvp_cfg_yuv422_320x240.FrameHeight / 2;
    display_size_byte = dvp_cfg_yuv422_320x240.FrameWidth / 2 * dvp_cfg_yuv422_320x240.FrameHeight / 2 * 3;

    /* malloc */
    dvp_buf1 = tiny_malloc(yuv422_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dvp_buf1, error1);
    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, yuv422_size_byte);

    dvp_buf2 = tiny_malloc(yuv422_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dvp_buf2, error1);
    VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, yuv422_size_byte);

    y8_buf = tiny_malloc(pixel_size);
    CHECK_POINT_NOT_NULL_EXIT(y8_buf, error1);
    VIDEO_LOG("y8_buf=0x%08x size=0x%x byte", y8_buf, pixel_size);

    rgb888_buf = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(rgb888_buf, error1);
    VIDEO_LOG("rgb888_buf=0x%08x size=0x%x byte", rgb888_buf, rgb888_size_byte);

    display_buf = tiny_malloc(display_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(display_buf, error1);
    VIDEO_LOG("display_buf=0x%08x size=0x%x byte", display_buf, display_size_byte);

    blender_mask_buf = tiny_malloc(display_size);
    CHECK_POINT_NOT_NULL_EXIT(blender_mask_buf, error1);
    VIDEO_LOG("blender_mask_buf=0x%08x size=0x%x byte", blender_mask_buf, display_size);

    memset(blender_mask_buf, 0, display_size);
    raw8_line_color(blender_mask_buf, 150, 250, 100, 100, 320, 0xFF);
    raw8_line_color(blender_mask_buf, 150, 250, 200, 200, 320, 0xFF);
    raw8_line_color(blender_mask_buf, 150, 150, 100, 200, 320, 0xFF);
    raw8_line_color(blender_mask_buf, 250, 250, 100, 200, 320, 0xFF);

    /* gpdma init */
    ret = GPDMA_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    if(true == dvp_pipo_en)
    {
        dvp_gpdma_cfg.src_mode = address_mode_pipo;
        dvp_gpdma_cfg.dst_mode = address_mode_pipo;
    }
    ret = GPDMA_Config(&dvp_gpdma_cfg, dvp_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = GPDMA_Config(&qspi_out_gpdma_cfg, qspi_out_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = DMA2D_Config(&gpdma2d_yuv422_rgb888_cfg, gpdma_2d_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_rgb888_cfg.dma_ch, &gpdma2d_img_yuv422_rgb888_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Config(&gpdma2d_yuv422_y8_cfg, gpdma_2d_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_y8_cfg.dma_ch, &gpdma2d_img_yuv422_y8_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Config(&gpdma2d_zoom_cfg, gpdma_2d_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(gpdma2d_zoom_cfg.dma_ch, &gpdma2d_img_zoom_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = GPDMA_Config(&blender_back_gpdma_cfg, blender_back_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_mask_gpdma_cfg, blender_mask_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_out_gpdma_cfg, blender_out_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* dvp init */
    ret = dvp_init(&dvp_cfg_yuv422_320x240, TEST_PIPE_DVP_MCLK_HZ);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* camera init colorbar*/
    camera_init(&camera_cfg_yuv422_320x240);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    lcd_qspi_out_pinmux();
    lcd_qspi_out_reset();
    lcd_qspi_out_bl_enable();

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* qspi_out init */
    qspi_out_cfg.format = QSPI_LCD_FORMAT_RGB888;
    if(qspi_out_cfg.txio == QSPI_LCD_TXIO_DMA) {
        dma_en = true;
    }
    qspi_out_cfg.txio = QSPI_LCD_TXIO_PIO;
    ret = qspi_lcd_init(&qspi_out_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* LCD SPD2010 init */
    if(qspi_out_cfg.format == QSPI_LCD_FORMAT_RGB565) {
        lcd_init(LCD_FORMAT_RGB565);
    } else {
        lcd_init(LCD_FORMAT_RGB888);
    }
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* lcd display clear */
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB888_RED, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);            // Error after deletion
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB888_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB888_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* blender init */
    blender_cfg.color = 0xFF0000;   // B8:G8:R8
    blender_cfg.alpha = 0x80;
    blender_cfg.back_format = BLENDER_BACK_FORMAT_RGB888;
    blender_cfg.blender_mode = BLENDER_MODE_FILL;
    blender_cfg.alpha_mode = BLENDER_ALPHA_MODE_1;
    ret = Blender_Initialize(Blender0(), &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* dvp start */
    ret = dvp_start();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    image_cnt = test_dvp_gpdma_finish_cnt;
    display_cnt = 0;
    blender_cnt = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    mmio_write32(GPDMA_BASE + 0x1B4, (1 << TEST_PIPE_DVP_GPDMA_CH));

    /* DVP GPDMA start */
    if(true == dvp_pipo_en)
    {
        /* PO block len */
        mmio_write32(GPDMA_BASE + 0x274 + 0x04 * TEST_PIPE_DVP_GPDMA_CH, yuv422_size_byte / sizeof(uint32_t));
        ret = GPDMA_Start_PiPo(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, (void*)VIC_BUF, dvp_buf1, dvp_buf2, yuv422_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    }
    else
    {
        ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, dvp_buf1, yuv422_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    }

    while(1)
    {
        /* dvp get new frame */
        if (image_cnt != test_dvp_gpdma_finish_cnt)
        {
            /* dvp buf */
            if(true == dvp_pipo_en)
            {
                image_cnt = test_dvp_gpdma_finish_cnt;
                VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());

                if(image_cnt & 0x1) {
                    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, yuv422_size_byte);
                    yuv422_buf = dvp_buf1;
                } else {
                    VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, yuv422_size_byte);
                    yuv422_buf = dvp_buf2;
                }
            }
            else
            {
                ret = dvp_stop();
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

                image_cnt = test_dvp_gpdma_finish_cnt;
                VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());
                yuv422_buf = dvp_buf1;
                VIDEO_LOG("yuv422_buf=0x%08x size=0x%x byte", yuv422_buf, yuv422_size_byte);
            }


            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_rgb888_cfg.dma_ch));

            /* 2D: YUV422 to RGB888 */
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            ret = DMA2D_Start_Normal(gpdma2d_yuv422_rgb888_cfg.dma_ch, yuv422_buf, rgb888_buf, yuv422_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(gpdma2d_cnt, test_gpdma_2d_finish_cnt, timeout, error2);
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            VIDEO_LOG("gpdma2d_cnt=%d", gpdma2d_cnt);
            VIDEO_LOG("rgb888_buf=0x%08x size=0x%x byte", rgb888_buf, rgb888_size_byte);

            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_rgb888_cfg.dma_ch));

            /* 2D: YUV422 to RGB888 config */
            ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_rgb888_cfg.dma_ch, &gpdma2d_img_yuv422_rgb888_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);


            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_y8_cfg.dma_ch));

            /* 2D: YUV422 to Y8 */
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            ret = DMA2D_Start_Normal(gpdma2d_yuv422_y8_cfg.dma_ch, yuv422_buf, y8_buf, yuv422_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(gpdma2d_cnt, test_gpdma_2d_finish_cnt, timeout, error2);
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            VIDEO_LOG("gpdma2d_cnt=%d", gpdma2d_cnt);
            VIDEO_LOG("y8_buf=0x%08x size=0x%x byte", y8_buf, pixel_size);

            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_y8_cfg.dma_ch));

            /* 2D: YUV422 to Y8 config */
            ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_y8_cfg.dma_ch, &gpdma2d_img_yuv422_y8_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

#if 1
            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_zoom_cfg.dma_ch));

            /* 2D: ZOOM 1/2 */
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            ret = DMA2D_Start_Normal(gpdma2d_zoom_cfg.dma_ch, rgb888_buf, display_buf, rgb888_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(gpdma2d_cnt, test_gpdma_2d_finish_cnt, timeout, error2);
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            VIDEO_LOG("gpdma2d_cnt=%d", gpdma2d_cnt);
            VIDEO_LOG("display_buf=0x%08x size=0x%x byte", display_buf, display_size_byte);

            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_zoom_cfg.dma_ch));

            /* 2D: ZOOM 1/2 config */
            ret = DMA2D_Image_Config_Extend(gpdma2d_zoom_cfg.dma_ch, &gpdma2d_img_zoom_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
#endif

            /* blender */
            blender_cnt = test_blender_out_gpdma_finish_cnt;
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_BACK_DMA_CH, (uint32_t*)display_buf, (uint32_t*)D2BACK_BUF, display_size_byte / sizeof(uint32_t));
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_MASK_DMA_CH, (uint32_t*)blender_mask_buf, (uint32_t*)D2MASK_BUF, display_size / sizeof(uint32_t));
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_OUT_DMA_CH, (uint32_t*)D2OUT_BUF, (uint32_t*)display_buf, display_size_byte / sizeof(uint32_t));
            ret = Blender_Start(Blender0());
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(blender_cnt, test_blender_out_gpdma_finish_cnt, timeout, error2);
            blender_cnt = test_blender_out_gpdma_finish_cnt;
            VIDEO_LOG("blender_cnt=%d", blender_cnt);


            /* qspi out */
            VIDEO_LOG("display_buf=0x%08x size=0x%x byte", display_buf, display_size_byte);
            //ret = qspi_lcd_init(&qspi_out_cfg);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            ret = test_pipe_qspi_out_spd2010_320x240_image_flush(display_buf, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            VIDEO_LOG("display_cnt=%d", display_cnt++);


            /* dvp start */
            if(true != dvp_pipo_en)
            {
                ret = dvp_start();
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
                mmio_write32(GPDMA_BASE + 0x1B4, (1 << TEST_PIPE_DVP_GPDMA_CH));
                ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, dvp_buf1, yuv422_size_byte / sizeof(uint32_t));
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            }
        }
    }

    ret = SUCCESS;

error2:
    dvp_reg_dump();
    qspi_lcd_reg_dump();
    blender_reg_dump();
    gpdma_reg_dump(TEST_PIPE_DVP_GPDMA_CH);
    gpdma_reg_dump(TEST_PIPE_QSPI_OUT_GPDMA_CH);
    gpdma_reg_dump(TEST_PIPE_D2BLENDER_BACK_DMA_CH);
    gpdma_reg_dump(TEST_PIPE_D2BLENDER_OUT_DMA_CH);
    dvp_stop();
    dvp_deinit();
    Blender_Stop(Blender0());
    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, yuv422_size_byte);
    VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, yuv422_size_byte);

error1:
    tiny_free(dvp_buf1);
    tiny_free(dvp_buf2);
    tiny_free(y8_buf);
    tiny_free(rgb888_buf);
    tiny_free(blender_mask_buf);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* camera 1920x1080 YUV422 -> DVP PSRAM */
static int32_t test_pipe_01_07(void)
{
    int32_t ret = FAILURE;
    uint32_t run_times = 0;
    bool dma_en = false;
    bool dvp_pipo_en = true;        // true false
    uint8_t *dvp_buf1 = NULL;
    uint8_t *dvp_buf2 = NULL;
    uint8_t *yuv422_buf = NULL;
    uint32_t yuv422_size_byte = 0;
    uint32_t image_cnt = 0;

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    camera_cfg_yuv422_320x240.frame_size = FRAMESIZE_FHD;
    dvp_cfg_yuv422_320x240.PixelOffset = 0;
    dvp_cfg_yuv422_320x240.LineOffset = 0;
    dvp_cfg_yuv422_320x240.FrameWidth = 1920;
    dvp_cfg_yuv422_320x240.FrameHeight = 1080;
    dvp_cfg_yuv422_320x240.DataAlign = DVP_DATA_ALIGN_LEFT;  // DVP_DATA_ALIGN_LEFT(bit11~4) / DVP_DATA_ALIGN_RIGHT(bit7~0)
    dvp_cfg_yuv422_320x240.PCKPolarity = DVP_POL_RISING;
    dvp_cfg_yuv422_320x240.VSPolarity = DVP_POL_RISING;
    dvp_cfg_yuv422_320x240.HSPolarity = DVP_POL_RISING;

#if 1   // YUV422
    dvp_cfg_yuv422_320x240.InputFormat = DVP_INPUT_FORM_YUV422_CBY0CRY1;
    yuv422_size_byte = dvp_cfg_yuv422_320x240.FrameWidth * dvp_cfg_yuv422_320x240.FrameHeight * 2;
#else   // RAW8
    dvp_cfg_yuv422_320x240.InputFormat = DVP_INPUT_FORM_LUMINA_8BIT;
    dvp_cfg_yuv422_320x240.FrameWidth = dvp_cfg_yuv422_320x240.FrameWidth * 2;
    yuv422_size_byte = dvp_cfg_yuv422_320x240.FrameWidth * dvp_cfg_yuv422_320x240.FrameHeight;
#endif
    dvp_buf1 = (uint8_t *)MEM_PSRAM_ADDR;
    dvp_buf2 = (uint8_t *)MEM_PSRAM_ADDR;
    yuv422_buf = (uint8_t *)MEM_PSRAM_ADDR;

    camera_reset();
    camera_pwdn();
    dvp_reset();

    /* psram init */
    //PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* gpdma init */
    ret = GPDMA_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    if(true == dvp_pipo_en)
    {
        dvp_gpdma_cfg.src_mode = address_mode_pipo;
        dvp_gpdma_cfg.dst_mode = address_mode_pipo;
    }
    ret = GPDMA_Config(&dvp_gpdma_cfg, dvp_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    /* dvp init */
    ret = dvp_init(&dvp_cfg_yuv422_320x240, TEST_PIPE_DVP_MCLK_HZ);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* camera init colorbar*/
    camera_init(&camera_cfg_yuv422_320x240);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* dvp start */
    ret = dvp_start();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* FIFO clear */
    mmio_write32(GPDMA_BASE + 0x1B4, (1 << TEST_PIPE_DVP_GPDMA_CH));
    test_dvp_gpdma_finish_cnt = 0;
    image_cnt = test_dvp_gpdma_finish_cnt;

    /* DVP GPDMA start */
    if(true == dvp_pipo_en)
    {
        /* PO block len */
        mmio_write32(GPDMA_BASE + 0x274 + 0x04 * TEST_PIPE_DVP_GPDMA_CH, yuv422_size_byte / sizeof(uint32_t));
        ret = GPDMA_Start_PiPo(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, (void*)VIC_BUF, dvp_buf1, dvp_buf2, yuv422_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    }
    else
    {
        ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, dvp_buf1, yuv422_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    }

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1)
    {
        /* dvp get new frame */
        if (image_cnt != test_dvp_gpdma_finish_cnt)
        {
            /* dvp buf */
            if(true == dvp_pipo_en)
            {
                image_cnt = test_dvp_gpdma_finish_cnt;
                //VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());

                if(image_cnt & 0x1) {
                    //VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, yuv422_size_byte);
                    yuv422_buf = dvp_buf1;
                } else {
                    //VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, yuv422_size_byte);
                    yuv422_buf = dvp_buf2;
                }
            }
            else
            {
                ret = dvp_stop();
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

                image_cnt = test_dvp_gpdma_finish_cnt;
                VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());
                yuv422_buf = dvp_buf1;
                VIDEO_LOG("yuv422_buf=0x%08x size=0x%x byte", yuv422_buf, yuv422_size_byte);
            }

            //DELAY_MS(5000);

            /* dvp start */
            if(true != dvp_pipo_en)
            {
                ret = dvp_start();
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
                mmio_write32(GPDMA_BASE + 0x1B4, (1 << TEST_PIPE_DVP_GPDMA_CH));
                ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, dvp_buf1, yuv422_size_byte / sizeof(uint32_t));
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            }

            run_times++;
            VIDEO_LOG("run_times=%d", run_times);

#if 1
            if(run_times >= 160)
            {
                camera_pwd();
                DELAY_MS(1000);
                VIDEO_LOG("[%s:%d] test finish", __func__, __LINE__);
                break;
            }
#endif

#if 0
            if((run_times % 10) == 0)
            {
                camera_reset();
                camera_init(&camera_cfg_yuv422_320x240);
                VIDEO_LOG("[%s:%d] camera RESET", __func__, __LINE__);
            }
#endif

#if 0
            if((run_times % 10) == 0)
            {
                dvp_reset();
                VIDEO_LOG("[%s:%d] DVP RESET", __func__, __LINE__);

                ret = dvp_init(&dvp_cfg_yuv422_320x240, TEST_PIPE_DVP_MCLK_HZ);
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
                ret = dvp_start();
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            }
#endif

#if 0
            if((run_times % 10) == 0)
            {
                VIDEO_LOG("[%s:%d] GPDMA RESET", __func__, __LINE__);

                ret = GPDMA_Stop(TEST_PIPE_DVP_GPDMA_CH);
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

                ret = GPDMA_Config(&dvp_gpdma_cfg, dvp_gpdma_callback, NULL);
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

                /* FIFO clear */
                mmio_write32(GPDMA_BASE + 0x1B4, (1 << TEST_PIPE_DVP_GPDMA_CH));

                /* DVP GPDMA start */
                if(true == dvp_pipo_en)
                {
                    /* PO block len */
                    mmio_write32(GPDMA_BASE + 0x274 + 0x04 * TEST_PIPE_DVP_GPDMA_CH, yuv422_size_byte / sizeof(uint32_t));
                    ret = GPDMA_Start_PiPo(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, (void*)VIC_BUF, dvp_buf1, dvp_buf2, yuv422_size_byte / sizeof(uint32_t));
                    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
                }
                else
                {
                    ret = GPDMA_Start_Normal(TEST_PIPE_DVP_GPDMA_CH, (void*)VIC_BUF, dvp_buf1, yuv422_size_byte / sizeof(uint32_t));
                    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
                }
            }
#endif


        }
    }

    ret = SUCCESS;

error2:
    dvp_reg_dump();
    gpdma_reg_dump(TEST_PIPE_DVP_GPDMA_CH);
    dvp_stop();
    dvp_deinit();
    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, yuv422_size_byte);
    VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, yuv422_size_byte);

error1:

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/*  TEST_PIPE_01_08:  camera 1920x1080 YUV422 -> DVP -> yuv2rgb zoom 1/2 960x540 PSRAM -> crop 320x240 -> blender mask+color -> qspi_out */
static int32_t test_pipe_01_08(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    bool dma_en = false;
    bool dvp_pipo_en = false;
    uint8_t *dvp_buf1 = NULL;
    uint8_t *dvp_buf2 = NULL;
    uint8_t *rgb888_buf = NULL;
    uint8_t *blender_mask_buf = NULL;
    uint8_t *display_buf = NULL;
    uint32_t yuv422_size_byte = 0;
    uint32_t rgb888_size_byte = 0;
    uint32_t display_size_byte = 0;
    uint32_t display_size = 0;
    uint32_t image_cnt = 0;
    uint32_t gpdma2d_cnt = 0;
    uint32_t blender_cnt = 0;
    uint32_t display_cnt = 0;

    csk_dma2d_init_t gpdma2d_yuv422_rgb888_zoom_cfg = {
        .dma_ch = dma_2d_ch6,
        .burst_len = dma2d_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_p2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_fix,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = dvp_hs_num5,
    };

    csk_dma_2d_image_cfg_t gpdma2d_img_yuv422_rgb888_zoom_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = 1920,
        .img_height = 1080,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_xrgb,
        .img_zoom_scale = csk_image_zoom_scale_1_to_2,
        .img_rgb888_format = csk_image_rgb888_format,
    };

    csk_dma2d_init_t gpdma2d_crop_cfg = {
        .dma_ch = dma_2d_ch7,
        .burst_len = dma2d_burst_len_8spl,
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

    csk_dma_2d_image_cfg_t gpdma2d_img_crop_cfg = {
        .img_input_format = csk_image_format_xrgb,
        .img_width = 1920 / 2,
        .img_height = 1080 / 2,
        .start_col = 1 + ((1920 / 2) - 320) / 2,  // =321  // (img_cfg->end_col - img_cfg->start_col + 1) * 3 / sizeof(uint32_t); word
        .start_row = 1 + ((1080 / 2) - 240) / 2,  // =151
        .end_col = 320 + ((1920 / 2) - 320) / 2,  // =640
        .end_row = 240 + ((1080 / 2) - 240) / 2,  // =390
        .img_output_fromat_transfer = csk_image_format_transfer_xrgb_crop,
        .img_rgb888_format = csk_image_rgb888_format,
    };

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    camera_cfg_yuv422_320x240.frame_size = FRAMESIZE_FHD;
    dvp_cfg_yuv422_320x240.PixelOffset = 0;
    dvp_cfg_yuv422_320x240.LineOffset = 0;
    dvp_cfg_yuv422_320x240.FrameWidth = 1920;
    dvp_cfg_yuv422_320x240.FrameHeight = 1080;
    if(true == dvp_pipo_en)
    {
        gpdma2d_yuv422_rgb888_zoom_cfg.src_mode = address_mode_pipo;
        gpdma2d_yuv422_rgb888_zoom_cfg.dst_mode = address_mode_pipo;
    }

    /* csk_image_zoom_scale_1_to_2 */
    yuv422_size_byte = dvp_cfg_yuv422_320x240.FrameWidth * dvp_cfg_yuv422_320x240.FrameHeight * 2;
    rgb888_size_byte = dvp_cfg_yuv422_320x240.FrameWidth / 2 * dvp_cfg_yuv422_320x240.FrameHeight / 2 * 3;
    display_size_byte = 320 * 240 * 3;
    display_size = 320 * 240;

    camera_reset();
    camera_pwdn();
    dvp_reset();

    /* psram init */
    //ret = PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS
    //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    dvp_buf1 = tiny_malloc(rgb888_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(dvp_buf1, error1);
    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, rgb888_size_byte);

    if(true == dvp_pipo_en)
    {
        dvp_buf2 = tiny_malloc(rgb888_size_byte);
        CHECK_POINT_NOT_NULL_EXIT(dvp_buf2, error1);
        VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, rgb888_size_byte);
    }

    display_buf = tiny_malloc(display_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(display_buf, error1);
    VIDEO_LOG("display_buf=0x%08x size=0x%x byte", display_buf, display_size_byte);

    blender_mask_buf = tiny_malloc(display_size);
    CHECK_POINT_NOT_NULL_EXIT(blender_mask_buf, error1);
    VIDEO_LOG("blender_mask_buf=0x%08x size=0x%x byte", blender_mask_buf, display_size);

    memset(blender_mask_buf, 0, display_size);
    raw8_line_color(blender_mask_buf, 150, 250, 100, 100, 320, 0xFF);
    raw8_line_color(blender_mask_buf, 150, 250, 200, 200, 320, 0xFF);
    raw8_line_color(blender_mask_buf, 150, 150, 100, 200, 320, 0xFF);
    raw8_line_color(blender_mask_buf, 250, 250, 100, 200, 320, 0xFF);

    /* gpdma init */
    ret = GPDMA_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Config(&gpdma2d_yuv422_rgb888_zoom_cfg, gpdma_2d_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_rgb888_zoom_cfg.dma_ch, &gpdma2d_img_yuv422_rgb888_zoom_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Config(&gpdma2d_crop_cfg, gpdma_2d_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    ret = DMA2D_Image_Config_Extend(gpdma2d_crop_cfg.dma_ch, &gpdma2d_img_crop_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = GPDMA_Config(&blender_back_gpdma_cfg, blender_back_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_mask_gpdma_cfg, blender_mask_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&blender_out_gpdma_cfg, blender_out_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&qspi_out_gpdma_cfg, qspi_out_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    lcd_qspi_out_pinmux();
    lcd_qspi_out_reset();
    lcd_qspi_out_bl_enable();

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* qspi_out init */
    qspi_out_cfg.format = QSPI_LCD_FORMAT_RGB888;
    if(qspi_out_cfg.txio == QSPI_LCD_TXIO_DMA) {
        dma_en = true;
    }
    qspi_out_cfg.txio = QSPI_LCD_TXIO_PIO;
    ret = qspi_lcd_init(&qspi_out_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* LCD SPD2010 init */
    if(qspi_out_cfg.format == QSPI_LCD_FORMAT_RGB565) {
        lcd_init(LCD_FORMAT_RGB565);
    } else {
        lcd_init(LCD_FORMAT_RGB888);
    }
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* lcd display clear */
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB888_RED, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);            // Error after deletion
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB888_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    DELAY_MS(100);
    ret = test_pipe_qspi_out_spd2010_320x240_color_flush(RGB888_BLACK, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* blender init */
    blender_cfg.color = 0xFF0000;   // B8:G8:R8
    blender_cfg.alpha = 0x80;
    blender_cfg.back_format = BLENDER_BACK_FORMAT_RGB888;
    blender_cfg.blender_mode = BLENDER_MODE_FILL;
    blender_cfg.alpha_mode = BLENDER_ALPHA_MODE_1;
    ret = Blender_Initialize(Blender0(), &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    /* dvp init */
    ret = dvp_init(&dvp_cfg_yuv422_320x240, TEST_PIPE_DVP_MCLK_HZ);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* camera init colorbar*/
    camera_init(&camera_cfg_yuv422_320x240);
    DELAY_MS(100);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);


    /* FIFO clear */
    mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_rgb888_zoom_cfg.dma_ch));

    test_gpdma_2d_finish_cnt = 0;
    gpdma2d_cnt = test_gpdma_2d_finish_cnt;


    /* DVP GPDMA start */
    if(true == dvp_pipo_en)
    {
        /* PO block len */
        //mmio_write32(GPDMA_BASE + 0x274 + 0x04 * gpdma2d_yuv422_rgb888_zoom_cfg.dma_ch, yuv422_size_byte / sizeof(uint32_t));
        ret = DMA2D_Start_PiPo(gpdma2d_yuv422_rgb888_zoom_cfg.dma_ch, (void*)VIC_BUF, (void*)VIC_BUF, dvp_buf1, dvp_buf2, yuv422_size_byte / sizeof(uint32_t), rgb888_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    }
    else
    {
        ret = DMA2D_Start_Normal(gpdma2d_yuv422_rgb888_zoom_cfg.dma_ch, (void*)VIC_BUF, dvp_buf1, yuv422_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    }

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);


    /* dvp start */
    ret = dvp_start();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);


    while(1)
    {
        /* dvp get new frame */
        if (gpdma2d_cnt != test_gpdma_2d_finish_cnt)
        {
            image_cnt++;

            if(true == dvp_pipo_en)
            {
                gpdma2d_cnt = test_gpdma_2d_finish_cnt;
                VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());

                if(image_cnt & 0x1) {
                    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, rgb888_size_byte);
                    rgb888_buf = dvp_buf1;
                } else {
                    VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, rgb888_size_byte);
                    rgb888_buf = dvp_buf2;
                }
            }
            else
            {
                ret = dvp_stop();
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

                gpdma2d_cnt = test_gpdma_2d_finish_cnt;
                VIDEO_LOG("image_cnt=%d eof_cnt=%d", image_cnt, dvp_frame_get());
                rgb888_buf = dvp_buf1;
                VIDEO_LOG("rgb888_buf=0x%08x size=0x%x byte", rgb888_buf, rgb888_size_byte);
            }


            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_crop_cfg.dma_ch));

            /* crop */
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            //ret = DMA2D_Start_Normal(gpdma2d_crop_cfg.dma_ch, rgb888_buf, rgb888_crop_buf, rgb888_size_byte / sizeof(uint32_t));
            ret = DMA2D_Start_Normal(gpdma2d_crop_cfg.dma_ch, \
                    rgb888_buf + (gpdma2d_img_crop_cfg.start_col - 1) * 3 + (gpdma2d_img_crop_cfg.start_row - 1) * gpdma2d_img_crop_cfg.img_width * 3, \
                    display_buf, rgb888_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(gpdma2d_cnt, test_gpdma_2d_finish_cnt, timeout, error2);
            gpdma2d_cnt = test_gpdma_2d_finish_cnt;
            VIDEO_LOG("display_buf=0x%08x size=0x%x byte", display_buf, display_size_byte);

            /* FIFO clear */
            mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_crop_cfg.dma_ch));

            /* 2D: crop config */
            ret = DMA2D_Image_Config_Extend(gpdma2d_crop_cfg.dma_ch, &gpdma2d_img_crop_cfg);
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);


            /* blender */
            blender_cnt = test_blender_out_gpdma_finish_cnt;
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_BACK_DMA_CH, (uint32_t*)display_buf, (uint32_t*)D2BACK_BUF, display_size_byte / sizeof(uint32_t));
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_MASK_DMA_CH, (uint32_t*)blender_mask_buf, (uint32_t*)D2MASK_BUF, display_size / sizeof(uint32_t));
            ret = GPDMA_Start_Normal(TEST_PIPE_D2BLENDER_OUT_DMA_CH, (uint32_t*)D2OUT_BUF, (uint32_t*)display_buf, display_size_byte / sizeof(uint32_t));
            ret = Blender_Start(Blender0());
            timeout = 3000000;  // wait blender done, timeout=1000ms
            CHECK_EQ_TIMEOUT_EXIT(blender_cnt, test_blender_out_gpdma_finish_cnt, timeout, error2);
            blender_cnt = test_blender_out_gpdma_finish_cnt;
            VIDEO_LOG("blender_cnt=%d", blender_cnt);


            /* qspi out */
            VIDEO_LOG("display_buf=0x%08x size=0x%x byte", display_buf, display_size_byte);
            //ret = qspi_lcd_init(&qspi_out_cfg);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            ret = test_pipe_qspi_out_spd2010_320x240_image_flush(display_buf, qspi_out_cfg.format, qspi_out_cfg.lane_num, dma_en);
            //CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            VIDEO_LOG("display_cnt=%d", display_cnt++);


            //DELAY_MS(3000);
//            if(image_cnt >= 16)
//            {
//                break;
//            }

            /* dvp start */
            if(true != dvp_pipo_en)
            {
                ret = dvp_start();
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

                /* FIFO clear */
                mmio_write32(GPDMA_BASE + 0x1B4, (1 << gpdma2d_yuv422_rgb888_zoom_cfg.dma_ch));

                /* 2D: YUV422 to RGB888 config */
                ret = DMA2D_Image_Config_Extend(gpdma2d_yuv422_rgb888_zoom_cfg.dma_ch, &gpdma2d_img_yuv422_rgb888_zoom_cfg);
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);


                ret = DMA2D_Start_Normal(gpdma2d_yuv422_rgb888_zoom_cfg.dma_ch, (void*)VIC_BUF, dvp_buf1, yuv422_size_byte / sizeof(uint32_t));
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
            }
        }

//        timeout++;
//        DELAY_US(10);
//        if(timeout >= 300000)
//        {
//            timeout = 0;
//            dvp_reg_dump();
//            gpdma_reg_dump(gpdma2d_yuv422_rgb888_zoom_cfg.dma_ch);
//        }
    }

    ret = SUCCESS;

error2:
    dvp_reg_dump();
    gpdma_reg_dump(gpdma2d_yuv422_rgb888_zoom_cfg.dma_ch);
    dvp_stop();
    dvp_deinit();
    VIDEO_LOG("dvp_buf1=0x%08x size=0x%x byte", dvp_buf1, rgb888_size_byte);
    VIDEO_LOG("dvp_buf2=0x%08x size=0x%x byte", dvp_buf2, rgb888_size_byte);
    VIDEO_LOG("display_buf=0x%08x size=0x%x byte", display_buf, display_size_byte);

error1:
    free(dvp_buf1);
    if(true != dvp_pipo_en)
    {
        free(dvp_buf2);
    }
    free(display_buf);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_pipe_qspi_out_cmd_tx_buf(uint8_t *pbuf, uint32_t size_byte, bool dma_en)
{
    int32_t ret = FAILURE;
    uint32_t i;
    uint32_t timeout = 0;

    //VIDEO_LOG("[%s:%d] pbuf=0x%x size=%d Byte", __func__, __LINE__, pbuf, size_byte);
    CHECK_POINT_NOT_NULL(pbuf);

    /* malloc */
    if (0 != (size_byte % 4)) {
        VIDEO_LOG("[%s:%d] size_byte=%d No 4-byte alignment", __func__, __LINE__, size_byte);
    }
    if (size_byte > 0xFFFFFF) {
        VIDEO_LOG("[%s:%d] size_byte=%d is over 0xFFFFFF", __func__, __LINE__, size_byte);
    }

    if(true == dma_en)
    {
        qspi_lcd_set_dma_size(size_byte);
        qspi_lcd_dma_enable();

        test_qspi_out_gpdma_finish_cnt = 0;
        ret = GPDMA_Start_Normal(TEST_PIPE_QSPI_OUT_GPDMA_CH, pbuf, (void*)(QSPI_LCD_BASE + 0x2C), size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

        timeout = 3000000;  // wait GPDMA done, timeout=1000ms
        while(!test_qspi_out_gpdma_finish_cnt)
        {
            DELAY_US(1);
            if(timeout-- == 0)
            {
                VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
                ret = FAILURE;
                goto error0;
            }
        }
    }
    else
    {
        ret = qspi_lcd_write(pbuf, size_byte);
        if(ret != CSK_DRIVER_OK) {
            VIDEO_LOG("[%s:%d] ", __func__, __LINE__);
        }

        timeout = 3000000;  // wait done, timeout=1000ms
        while(!is_qspi_lcd_irq_tx_done())
        {
            DELAY_US(1);
            if(timeout-- == 0)
            {
                VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
                ret = FAILURE;
                goto error0;
            }
        }

//        for (i = 0; i < size_byte; i++)
//        {
//            ret = qspi_lcd_write(pbuf + i, 1);
//            //ret = qspi_lcd_write(pbuf + i + 12, 1);
//            if(ret != CSK_DRIVER_OK) {
//                VIDEO_LOG("[%s:%d] i=%d", __func__, __LINE__, i);
//            }
//
//            timeout = 3000000;  // wait done, timeout=1000ms
//            while(!is_qspi_lcd_irq_tx_done())
//            {
//                DELAY_US(1);
//                if(timeout-- == 0)
//                {
//                    VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
//                    ret = FAILURE;
//                    goto error0;
//                }
//            }
//        }
    }
    ret = SUCCESS;

error0:

    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    return ret;
}

static int32_t test_pipe_qspi_out_cmd_init(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint8_t lane_num, bool dma_en)
{
    int32_t ret = FAILURE;
    __attribute__ ((aligned(4))) static uint8_t buf[20] = {0};

    sw_qspi_cs_clr();
    buf[0] = 0x02;
    buf[1] = 0x00;
    buf[2] = 0x2A;
    buf[3] = 0x00;
    buf[4] = (xs>>8);
    buf[5] = (xs&0xff);
    buf[6] = ((xe-1)>>8);
    buf[7] = ((xe-1)&0xff);
    test_pipe_qspi_out_cmd_tx_buf(buf, 8, dma_en);
    sw_qspi_cs_set();

    sw_qspi_cs_clr();
    buf[0] = 0x02;
    buf[1] = 0x00;
    buf[2] = 0x2B;
    buf[3] = 0x00;
    buf[4] = (ys>>8);
    buf[5] = (ys&0xff);
    buf[6] = ((ye-1)>>8);
    buf[7] = ((ye-1)&0xff);
    test_pipe_qspi_out_cmd_tx_buf(buf, 8, dma_en);
    sw_qspi_cs_set();

    sw_qspi_cs_clr();
    buf[0] = 0x02;
    buf[1] = 0x00;
    buf[2] = 0x2C;
    buf[3] = 0x00;
    test_pipe_qspi_out_cmd_tx_buf(buf, 4, dma_en);
    sw_qspi_cs_set();

    sw_qspi_cs_clr();
    switch(lane_num)
    {
        case 1:
            buf[0] = 0x02;
            buf[1] = 0x00;
            buf[2] = 0x3C;
            buf[3] = 0x00;
            test_pipe_qspi_out_cmd_tx_buf(buf, 4, dma_en);
            break;

        case 4:
            buf[0] = 0x32;
            buf[1] = 0x00;
            buf[2] = 0x3C;
            buf[3] = 0x00;
            test_pipe_qspi_out_cmd_tx_buf(buf, 4, dma_en);
            break;

        default:
            break;
    }

    qspi_lcd_set_lane_num(lane_num);

    ret = SUCCESS;
    return ret;
}

static int32_t test_pipe_qspi_out_spd2010_line_flush(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint8_t *pbuf, uint32_t line_size_byte, uint8_t lane_num, bool dma_en)
{
    int32_t ret = FAILURE;
    uint32_t i;
    uint32_t timeout = 0;

    //VIDEO_LOG("[%s:%d] start(%d,%d) end(%d,%d)", __func__, __LINE__, xs, ys, xe, ye);
    //VIDEO_LOG("[%s:%d] pbuf=0x%x size=%d Byte", __func__, __LINE__, pbuf, line_size_byte);
    CHECK_POINT_NOT_NULL(pbuf);

    /* malloc */
    if (0 != (line_size_byte % 4)) {
        VIDEO_LOG("[%s:%d] line_size_byte=%d No 4-byte alignment", __func__, __LINE__, line_size_byte);
    }
    if (line_size_byte > 0xFFFFFF) {
        VIDEO_LOG("[%s:%d] line_size_byte=%d is over 0xFFFFFF", __func__, __LINE__, line_size_byte);
    }

#if 0
    lcd_spd2010_window_set(xs,ys,xe-1,ye-1);
    sw_qspi_cs_clr();
    lcd_spd2010_datalane_set(lane_num);
    qspi_lcd_set_lane_num(lane_num);
    DELAY_US(100);
#else
    test_pipe_qspi_out_cmd_init(xs, ys, xe, ye, lane_num, false);
#endif

    if(true == dma_en)
    {
        qspi_lcd_set_dma_size(line_size_byte);
        qspi_lcd_dma_enable();

        test_qspi_out_gpdma_finish_cnt = 0;
        ret = GPDMA_Start_Normal(TEST_PIPE_QSPI_OUT_GPDMA_CH, pbuf, (void*)(QSPI_LCD_BASE + 0x2C), line_size_byte / sizeof(uint32_t));
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

        timeout = 3000000;  // wait GPDMA done, timeout=1000ms
        while(!test_qspi_out_gpdma_finish_cnt)
        {
            DELAY_US(1);
            if(timeout-- == 0)
            {
                VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
                ret = FAILURE;
                goto error0;
            }
        }
    }
    else
    {
        for (i = 0; i < line_size_byte; i++)
        {
            ret = qspi_lcd_write(pbuf + i, 1);
            //ret = qspi_lcd_write(pbuf + i + 12, 1);
            if(ret != CSK_DRIVER_OK) {
                VIDEO_LOG("[%s:%d] i=%d", __func__, __LINE__, i);
            }

            timeout = 3000000;  // wait done, timeout=1000ms
            while(!is_qspi_lcd_irq_tx_done())
            {
                DELAY_US(1);
                if(timeout-- == 0)
                {
                    VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
                    ret = FAILURE;
                    goto error0;
                }
            }
        }

//        for (i=ys; i<ye; i++)
//        {
//            qspi_lcd_write(line_buf, line_size_byte);
//            while(!is_qspi_lcd_irq_tx_done());
//        }
    }
    ret = SUCCESS;

error0:
    sw_qspi_cs_set();
    qspi_lcd_dma_disable();
    qspi_lcd_set_lane_num(1);

    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    return ret;
}


static int32_t test_pipe_qspi_out_spd2010_320x240_image_flush(uint8_t *pbuf, qspi_lcd_format_t format, uint8_t lane_num, bool dma_en)
{
    int32_t ret = FAILURE;
    uint32_t i = 0;
    uint16_t xs = 46;
    uint16_t ys = 86;
    uint16_t xe = 320 + 46;
    uint16_t ye = 240 + 86;
    uint32_t line_size_byte = 0;

    switch(format)
    {
        case QSPI_LCD_FORMAT_RGB565:
            line_size_byte = 320 * 2;
            VIDEO_LOG("[%s:%d] RGB565", __func__, __LINE__);
            break;

        case QSPI_LCD_FORMAT_RGB888:
            line_size_byte = 320 * 3;
            VIDEO_LOG("[%s:%d] RGB888", __func__, __LINE__);
            break;

        default:
            VIDEO_LOG("[%s:%d] error format=%d", __func__, __LINE__, format);
            return FAILURE;
    }

    for (i = 0; i < (ye - ys); i++)
    {
        ret = test_pipe_qspi_out_spd2010_line_flush(46, ys + i, xe, ys + i + 1, pbuf + line_size_byte * i, line_size_byte, lane_num, dma_en);
    }

    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    return ret;
}


static int32_t test_pipe_qspi_out_spd2010_320x240_color_flush(uint32_t color, qspi_lcd_format_t format, uint8_t lane_num, bool dma_en)
{
    int32_t ret = FAILURE;
    uint32_t i = 0;
    uint16_t xs = 0;
    uint16_t ys = 0;
    uint16_t xe = 412;
    uint16_t ye = 412;
    uint32_t line_size_byte = 0;
    uint8_t *line_buf = NULL;
    uint8_t *pbuf = NULL;

    switch(format)
    {
        case QSPI_LCD_FORMAT_RGB565:
            line_size_byte = 412 * 2;
            line_buf = tiny_malloc(line_size_byte);
            CHECK_POINT_NOT_NULL(line_buf);
            VIDEO_LOG("image_buf=0x%08x size=0x%x byte", line_buf, line_size_byte);

            pbuf = line_buf;
            for (i = 0; i < (line_size_byte / 2); i++)
            {
                *pbuf++ = color & 0xFF;
                *pbuf++ = (color >> 8) & 0xFF;
            }
            break;

        case QSPI_LCD_FORMAT_RGB888:
            line_size_byte = 412 * 3;
            line_buf = tiny_malloc(line_size_byte);
            CHECK_POINT_NOT_NULL(line_buf);
            VIDEO_LOG("image_buf=0x%08x size=0x%x byte", line_buf, line_size_byte);

            pbuf = line_buf;
            for (i = 0; i < (line_size_byte / 3); i++)
            {
                *pbuf++ = color & 0xFF;
                *pbuf++ = (color >> 8) & 0xFF;
                *pbuf++ = (color >> 16) & 0xFF;
            }
            break;

        default:
            VIDEO_LOG("[%s:%d] error format=%d", __func__, __LINE__, format);
            return FAILURE;
    }

    pbuf = line_buf;
    for (i = 0; i < (ye - ys); i++)
    {
        ret = test_pipe_qspi_out_spd2010_line_flush(xs, ys + i, xe, ys + i + 1, pbuf, line_size_byte, lane_num, dma_en);
    }

    tiny_free(line_buf);

    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    return ret;
}



