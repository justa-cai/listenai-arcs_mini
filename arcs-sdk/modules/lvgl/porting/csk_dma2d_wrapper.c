#if 1
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

#include "sysheap.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define D2BLENDER_MASK_DMA_CH   gp_dma_ch5
#define D2BLENDER_BACK_DMA_CH   dma_2d_ch6
#define D2BLENDER_FORE_DMA_CH   dma_2d_ch7
#define D2BLENDER_OUT_DMA_CH    dma_2d_ch8

#define RGB565_PIXEL_BYTE   2
#define RGB888_PIXEL_BYTE   3


#define FAILURE -1
#define SUCCESS 0

#define CHECK_RET_EQ_EXIT(Ret, express, errExit)\
    do{\
        if ((express) != (Ret))\
        {\
            CLOGE("ret %d not equal with %d failed at %s: LINE: %d", (Ret), (express), __FUNCTION__, __LINE__);\
            goto errExit;\
        }\
    }while(0)

#define CHECK_RET_EQ(Ret, express)\
    do{\
        if ((express) != (Ret))\
        {\
            CLOG("ret %d not equal with %d failed at %s: LINE: %d", (Ret), (express), __FUNCTION__, __LINE__);\
            return (Ret);\
        }\
    }while(0)

typedef enum _csk_dma2d_format {
    DMA2D_FORMAT_RGB565 = 0,
    DMA2D_FORMAT_RGB888,
    DMA2D_FORMAT_BUTT,
} csk_dma2d_format_t;

typedef enum {
    LVGL_DMA2D_COPY = 0,
    LVGL_DMA2D_FILL,
    LVGL_DMA2D_FILL_MASK,
    LVGL_DMA2D_BLEND,
    LVGL_DMA2D_BUTT,
} lvgl_dma2d_mode_e;

typedef struct
{
    lvgl_dma2d_mode_e mode;
    void *buf;
    void *map;
    void *mask;
    uint16_t buf_w;
    uint16_t map_w;
    uint16_t copy_w;
    uint16_t copy_h;
    uint32_t color;     // RGB888
    uint8_t opa;
}lvgl_dma2d_cfg_t;

static SemaphoreHandle_t dma_sem = NULL;

int32_t rgb565_dma2d_init(void);
int32_t rgb565_dma2d_config(lvgl_dma2d_cfg_t *pcfg);

int32_t lv_gpu_dma2d_copy(void *buf, uint16_t buf_w, void *map, uint16_t map_w, uint16_t copy_w, uint16_t copy_h);
int32_t lv_gpu_dma2d_fill(void *buf, uint16_t buf_w, uint32_t color, uint16_t fill_w, uint16_t fill_h);
int32_t lv_gpu_dma2d_fill_mask(void *buf, uint16_t buf_w, uint32_t color, void *mask, uint8_t opa, uint16_t fill_w, uint16_t fill_h);
int32_t lv_gpu_dma2d_blend(void *buf, uint16_t buf_w, void *map, uint8_t opa, uint16_t map_w, uint16_t copy_w, uint16_t copy_h);

//  int32_t test_lv_rgb565_copy(void);    // src step, dts step
//  int32_t test_lv_rgb565_fill(void);
//  int32_t test_lv_rgb565_fill_mask(void);
//  int32_t test_lv_rgb565_blend(void);


// 时间测量函数 - 返回微秒
static inline uint32_t get_time_us(void)
{
	uint64_t cur_tick = SysTimer_GetLoadValue();
	return (uint32_t)(cur_tick * 1000 * 1000 / CRM_GetMtimeFreq());    
}

// 计算耗时（微秒）
static inline uint32_t calc_time_elapsed_us(uint32_t start_time)
{
	return get_time_us() - start_time;
}


 volatile uint32_t blender_dma_finish_flag = 0;

 void d2blender_out_dma_callback(uint32_t event, void* workspace)
{
    // CLOG("[%s:%d] event=%d", __func__, __LINE__, event);
    blender_dma_finish_flag++;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(dma_sem, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


 void DMA2D_Step_Config(csk_dma2d_ch_t dma_ch, csk_dma2d_format_t format, uint16_t src_width, uint16_t dts_width, uint16_t height)
{
    uint8_t pixel_byte = 0;

    switch(format)
    {
        case DMA2D_FORMAT_RGB565:
            pixel_byte = RGB565_PIXEL_BYTE;
            break;

        case DMA2D_FORMAT_RGB888:
            pixel_byte = RGB888_PIXEL_BYTE;
            break;

        default:
            break;
    }

    switch(dma_ch)
    {
        case dma_2d_ch6:
            IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS |= 0x1;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH6.bit.CFG_D2_ADDR_STEP_S_CH6 = 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH6.bit.CFG_D2_ADDR_STEP_L0_CH6 = ((dts_width - src_width) * pixel_byte) + 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM_CH6 = 1;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM0_CH6 = height;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN6 = 0;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN6 = 1;
            break;

        case dma_2d_ch7:
            IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS |= 0x2;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH7.bit.CFG_D2_ADDR_STEP_S_CH7 = 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH7.bit.CFG_D2_ADDR_STEP_L0_CH7 = ((dts_width - src_width) * pixel_byte) + 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM_CH7 = 1;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM0_CH7 = height;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN7 = 0;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN7 = 1;
            break;

        case dma_2d_ch8:
            IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS |= 0x4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH8.bit.CFG_D2_ADDR_STEP_S_CH8 = 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH8.bit.CFG_D2_ADDR_STEP_L0_CH8 = ((dts_width - src_width) * pixel_byte) + 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM_CH8 = 1;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM0_CH8 = height;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN8 = 0;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN8 = 1;
            break;

        case dma_2d_ch9:
            IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS |= 0x8;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9 = ((dts_width - src_width) * pixel_byte) + 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 1;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = height;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN9 = 0;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN9 = 1;
            break;

        default:
            break;
    }
}

int32_t rgb565_dma2d_init(void)
{
    int32_t ret = FAILURE;

    CLOGD("DMA2D INIT\n");

    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_BLENDER_CLK = 0x1; // blender clk enable

    // ret = GPDMA_Initialize();
    // CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

	// Create binary semaphore for DMA synchronization
	dma_sem = xSemaphoreCreateBinary();
	if (dma_sem == NULL) {
		CLOGE("[%s] Failed to create DMA semaphore", __FUNCTION__);
		return -1;
	}

    // xSemaphoreGive(dma_sem);

    return ret;
}


 int32_t rgb565_dma2d_config(lvgl_dma2d_cfg_t *pcfg)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t back_size_byte = 0;
    uint32_t fore_size_byte = 0;
    uint32_t mask_size_byte = 0;
    uint32_t out_size_byte = 0;
    bool is_give_mutex = false;
#if CONFIG_LVGL_GPU_CSK_GPDMA_DMA2D_MUTEX
    extern SemaphoreHandle_t gpdma_mutex;

	if (xSemaphoreTake(gpdma_mutex, portMAX_DELAY) != pdTRUE) {
		CLOGE("[%s] Failed to take TE semaphore", __FUNCTION__);
		return -1;
	}
#endif
    void *blender_dev = Blender0();

     Blender_InitTypeDef blender_cfg = {
            .blender_mode = BLENDER_MODE_FILL,
            .alpha_mode = BLENDER_ALPHA_MODE_2,
            .back_format = BLENDER_BACK_FORMAT_RGB565,
            .fore_format = BLENDER_FORE_FORMAT_RGB565,
            .img_width = 0,
            .img_height = 0,
            .color = 0xFFFFFF,
            .alpha = 0xFF,
            .burst_thd = 8,
    };

     csk_dma2d_init_t d2back_input = {
        .dma_ch = D2BLENDER_BACK_DMA_CH,
        .burst_len = dma2d_burst_len_2spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2p,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = d2back_hs_num3,
    };

     csk_dma_2d_image_cfg_t d2back_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = 0,
        .img_height = 0,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 0,
        .end_row = 0,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    };

     csk_dma2d_init_t d2fore_input = {
        .dma_ch = D2BLENDER_FORE_DMA_CH,
        .burst_len = dma2d_burst_len_2spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2p,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = d2fore_hs_num4,
    };

     csk_dma_2d_image_cfg_t d2fore_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = 0,
        .img_height = 0,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 0,
        .end_row = 0,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    };

     csk_gpdma_init_t d2mask_input = {
            .dma_ch = D2BLENDER_MASK_DMA_CH,
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

     csk_dma2d_init_t d2out_output = {
        .dma_ch = D2BLENDER_OUT_DMA_CH,
        .burst_len = dma2d_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_p2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_fix,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = d2out_hs_num1,
    };

     csk_dma_2d_image_cfg_t d2out_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = 0,
        .img_height = 0,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 0,
        .end_row = 0,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    };

    IP_AP_CFG->REG_SW_RESET.bit.BLENDER_RESET = 0x1; // blender reset
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << D2BLENDER_MASK_DMA_CH);
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << D2BLENDER_BACK_DMA_CH);
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << D2BLENDER_FORE_DMA_CH);
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << D2BLENDER_OUT_DMA_CH);

    d2back_img_cfg.img_width = pcfg->buf_w;
    d2back_img_cfg.img_height = pcfg->copy_h;
    d2back_img_cfg.end_col = pcfg->copy_w;
    d2back_img_cfg.end_row = pcfg->copy_h;

    d2fore_img_cfg.img_width = pcfg->map_w;
    d2fore_img_cfg.img_height = pcfg->copy_h;
    d2fore_img_cfg.end_col = pcfg->copy_w;
    d2fore_img_cfg.end_row = pcfg->copy_h;

    d2out_img_cfg.img_width = pcfg->buf_w;
    d2out_img_cfg.img_height = pcfg->copy_h;
    d2out_img_cfg.end_col = pcfg->copy_w;
    d2out_img_cfg.end_row = pcfg->copy_h;

    back_size_byte = pcfg->buf_w * pcfg->copy_h * RGB565_PIXEL_BYTE;
    fore_size_byte = pcfg->map_w * pcfg->copy_h * RGB565_PIXEL_BYTE;
    mask_size_byte = pcfg->copy_w * pcfg->copy_h;
    out_size_byte = pcfg->copy_w * pcfg->copy_h * RGB565_PIXEL_BYTE;

    blender_cfg.img_width = pcfg->copy_w;
    blender_cfg.img_height = pcfg->copy_h;
    blender_cfg.color = pcfg->color;
    blender_cfg.alpha = pcfg->opa;

    //CLOGD("[%s:%d] RGB888 blender_cfg.color=0x%x", __func__, __LINE__, blender_cfg.color);

    switch(pcfg->mode)
    {
        case LVGL_DMA2D_COPY:
        case LVGL_DMA2D_BLEND:
            blender_cfg.blender_mode = BLENDER_MODE_MAP;
            blender_cfg.alpha_mode = BLENDER_ALPHA_MODE_2;
            break;

        case LVGL_DMA2D_FILL:
            blender_cfg.blender_mode = BLENDER_MODE_FILL;
            blender_cfg.alpha_mode = BLENDER_ALPHA_MODE_2;
            break;

        case LVGL_DMA2D_FILL_MASK:
            blender_cfg.blender_mode = BLENDER_MODE_FILL;
            blender_cfg.alpha_mode = BLENDER_ALPHA_MODE_1;
            break;

        default:
            break;
    }

    /* blender init */
    ret = Blender_Initialize(blender_dev, &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    /* GPDMA init and start */
    blender_dma_finish_flag = 0;
    switch(pcfg->mode)
    {
        case LVGL_DMA2D_COPY:
        case LVGL_DMA2D_BLEND:
            ret = DMA2D_Config(&d2fore_input, NULL, NULL);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Image_Config_Extend(d2fore_input.dma_ch, &d2fore_img_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Start_Normal(d2fore_input.dma_ch, pcfg->map, (uint32_t*)D2FORE_BUF, fore_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
            break;

        case LVGL_DMA2D_FILL_MASK:
            ret = GPDMA_Config(&d2mask_input, NULL, NULL);
            CHECK_RET_EQ(ret, CSK_DRIVER_OK);

            ret = GPDMA_Start_Normal(d2mask_input.dma_ch, pcfg->mask, (uint32_t*)D2MASK_BUF, mask_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ(ret, CSK_DRIVER_OK);
            break;

        case LVGL_DMA2D_FILL:
            break;

        default:
            break;
    }

    ret = DMA2D_Config(&d2back_input, NULL, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = DMA2D_Image_Config_Extend(d2back_input.dma_ch, &d2back_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = DMA2D_Config(&d2out_output, d2blender_out_dma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = DMA2D_Image_Config_Extend(d2out_output.dma_ch, &d2out_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    DMA2D_Step_Config(d2out_output.dma_ch, DMA2D_FORMAT_RGB565, pcfg->copy_w, pcfg->buf_w, pcfg->copy_h);

    ret = DMA2D_Start_Normal(d2back_input.dma_ch, pcfg->buf, (uint32_t*)D2BACK_BUF, back_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = DMA2D_Start_Normal(d2out_output.dma_ch, (uint32_t*)D2OUT_BUF, pcfg->buf, out_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
#if CONFIG_LVGL_GPU_CSK_GPDMA_DMA2D_MUTEX
    xSemaphoreGive(gpdma_mutex);
    is_give_mutex = true;
#endif
    /* blender start */
    Blender_Start(blender_dev);

    timeout = 200000;  // wait blender done, timeout=1000ms
    uint32_t start = get_time_us();
#if 0
    while(!blender_dma_finish_flag)
    {
        if(timeout-- == 0)
        {
            CLOG("[%s:%d] wait timeout", __func__, __LINE__);
            CLOGD("func:%s, mode:%d, buf:0x%x, map:0x%x, mask:0x%x, buf_w:%d, map_w:%d, copy_w:%d, copy_h:%d, color:0x%x, opa:%d\n", __FUNCTION__, pcfg->mode, pcfg->buf, pcfg->map, pcfg->mask, pcfg->buf_w, pcfg->map_w, pcfg->copy_w, pcfg->copy_h, pcfg->color, pcfg->opa);
            // blender_reg_dump();
            ret = FAILURE;
            goto error;
        }
    }
#else
	if (xSemaphoreTake(dma_sem, pdMS_TO_TICKS(20)) != pdTRUE) {
		// CLOGE("[%s] Failed to take DMA2D semaphore, mode:%d", __FUNCTION__, pcfg->mode);
        CLOGD("func:%s, mode:%d, buf:0x%x, map:0x%x, mask:0x%x, buf_w:%d, map_w:%d, copy_w:%d, copy_h:%d, color:0x%x, opa:%d\n", __FUNCTION__, pcfg->mode, pcfg->buf, pcfg->map, pcfg->mask, pcfg->buf_w, pcfg->map_w, pcfg->copy_w, pcfg->copy_h, pcfg->color, pcfg->opa);
        goto error;
	}
#endif
    uint32_t ela = calc_time_elapsed_us(start);
    // CLOGD("[%s:%d] jun wait blender done, time=%dus, timeout=%d", __func__, __LINE__, ela, timeout);

    // Blender_Stop(blender_dev);

    ret = SUCCESS;

error:
#if CONFIG_LVGL_GPU_CSK_GPDMA_DMA2D_MUTEX
    if (is_give_mutex == false) {
        xSemaphoreGive(gpdma_mutex);
    }
#endif
    DMA2D_Stop(d2back_input.dma_ch);
    DMA2D_Stop(d2out_output.dma_ch);

    switch(pcfg->mode)
    {
        case LVGL_DMA2D_COPY:
        case LVGL_DMA2D_BLEND:
             DMA2D_Stop(d2fore_input.dma_ch);
            break;

        case LVGL_DMA2D_FILL_MASK:
            GPDMA_Stop(d2mask_input.dma_ch);
            break;

        case LVGL_DMA2D_FILL:
            break;

        default:
            break;
    }

    Blender_Stop(blender_dev);    
    return ret;
}


int32_t lv_gpu_dma2d_copy(void *buf, uint16_t buf_w, void *map, uint16_t map_w, uint16_t copy_w, uint16_t copy_h)
{
    int32_t ret = FAILURE;
    lvgl_dma2d_cfg_t lvgl_cfg = {0};

    lvgl_cfg.mode = LVGL_DMA2D_COPY;
    lvgl_cfg.buf = buf;
    lvgl_cfg.map = map;
    lvgl_cfg.mask = NULL;
    lvgl_cfg.buf_w = buf_w;
    lvgl_cfg.map_w = map_w;
    lvgl_cfg.copy_w = copy_w;
    lvgl_cfg.copy_h = copy_h;
    lvgl_cfg.color = 0xFFFFFF;
    lvgl_cfg.opa = 0xFF;

    // CLOGD("func: %s ", __FUNCTION__);

    ret = rgb565_dma2d_config(&lvgl_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    // CHECK_RET_EQ(ret, SUCCESS);

error:

    return ret;
}


 int32_t lv_gpu_dma2d_fill(void *buf, uint16_t buf_w, uint32_t color, uint16_t fill_w, uint16_t fill_h)
{
    int32_t ret = FAILURE;
    lvgl_dma2d_cfg_t lvgl_cfg = {0};
    uint32_t rgb888 = 0;

#if 0
    rgb888 = ((((color & 0xF800) >> 8) | ((color & 0xF800) >> 13)) << 16) | \
             ((((color & 0x07E0) >> 3) | ((color & 0x07E0) >> 9)) << 8 ) | \
             (((color & 0x001F) << 3) | ((color & 0x001F) >> 2));
#elif 0
    rgb888 = (((color & 0xF800) << 8) | ((color & 0x07E0) << 5 ) | ((color & 0x001F) << 3));
#else
    rgb888 = ((((color & 0xF800) >> 8) | ((color & 0xF800) >> 13))) |         /* R: 高5位转到低8位 */ \
             ((((color & 0x07E0) >> 3) | ((color & 0x07E0) >> 9)) << 8) |     /* G: 中间6位转到中间8位 */ \
             ((((color & 0x001F) << 3) | ((color & 0x001F) >> 2)) << 16);     /* B: 低5位转到高8位 */
#endif

    lvgl_cfg.mode = LVGL_DMA2D_FILL;
    lvgl_cfg.buf = buf;
    lvgl_cfg.map = NULL;
    lvgl_cfg.mask = NULL;
    lvgl_cfg.buf_w = buf_w;
    lvgl_cfg.map_w = 0;
    lvgl_cfg.copy_w = fill_w;
    lvgl_cfg.copy_h = fill_h;
    lvgl_cfg.color = rgb888;    // RGB888
    lvgl_cfg.opa = 0xFF;

    // CLOGD("func:%s,mode:%d, buf:0x%x, temp_buf:0x%x, map:0x%x, mask:0x%x, buf_w:%d, map_w:%d, fill_w:%d, copy_w:%d, copy_h:%d, color:0x%x, opa:%d\n",
    //         __FUNCTION__, lvgl_cfg.mode, buf, lvgl_cfg.buf, lvgl_cfg.map, lvgl_cfg.mask, lvgl_cfg.buf_w, lvgl_cfg.map_w, fill_w,lvgl_cfg.copy_w, lvgl_cfg.copy_h, lvgl_cfg.color, lvgl_cfg.opa);

    // CLOGD("func: %s ", __FUNCTION__);

    ret = rgb565_dma2d_config(&lvgl_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    // CHECK_RET_EQ(ret, SUCCESS);
error:

    // CLOGD("func:%s, mode:%d, ret:%d\n", __FUNCTION__, lvgl_cfg.mode, ret);

    return ret;
}


 int32_t lv_gpu_dma2d_fill_mask(void *buf, uint16_t buf_w, uint32_t color, void *mask, uint8_t opa, uint16_t fill_w, uint16_t fill_h)
{
    int32_t ret = FAILURE;
    lvgl_dma2d_cfg_t lvgl_cfg = {0};
    uint32_t rgb888 = 0;

#if 0
    rgb888 = ((((color & 0xF800) >> 8) | ((color & 0xF800) >> 13)) << 16) | \
             ((((color & 0x07E0) >> 3) | ((color & 0x07E0) >> 9)) << 8 ) | \
             (((color & 0x001F) << 3) | ((color & 0x001F) >> 2));
#elif 0
    rgb888 = (((color & 0xF800) << 8) | ((color & 0x07E0) << 5 ) | ((color & 0x001F) << 3));
#else
    rgb888 = ((((color & 0xF800) >> 8) | ((color & 0xF800) >> 13))) |         /* R: 高5位转到低8位 */ \
             ((((color & 0x07E0) >> 3) | ((color & 0x07E0) >> 9)) << 8) |     /* G: 中间6位转到中间8位 */ \
             ((((color & 0x001F) << 3) | ((color & 0x001F) >> 2)) << 16);     /* B: 低5位转到高8位 */
#endif

    lvgl_cfg.mode = LVGL_DMA2D_FILL_MASK;
    lvgl_cfg.buf = buf;
    lvgl_cfg.map = NULL;
    lvgl_cfg.mask = mask;
    lvgl_cfg.buf_w = buf_w;
    lvgl_cfg.map_w = 0;
    lvgl_cfg.copy_w = fill_w;
    lvgl_cfg.copy_h = fill_h;
    lvgl_cfg.color = rgb888;    // RGB888
    lvgl_cfg.opa = opa;

    // CLOGD("func: %s ", __FUNCTION__);

    ret = rgb565_dma2d_config(&lvgl_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    // CHECK_RET_EQ(ret, SUCCESS);
error:

    return ret;
}


 int32_t lv_gpu_dma2d_blend(void *buf, uint16_t buf_w, void *map, uint8_t opa, uint16_t map_w, uint16_t copy_w, uint16_t copy_h)
{
    int32_t ret = FAILURE;
    lvgl_dma2d_cfg_t lvgl_cfg = {0};

    if(copy_h == 0) {
        CLOG("func: %s, line: %d, copy_h == 0", __FUNCTION__, __LINE__);
        return -1;
    }

    lvgl_cfg.mode = LVGL_DMA2D_BLEND;
    lvgl_cfg.buf = buf;
    lvgl_cfg.map = map;
    lvgl_cfg.mask = NULL;
    lvgl_cfg.buf_w = buf_w;
    lvgl_cfg.map_w = map_w;
    lvgl_cfg.copy_w = copy_w;
    lvgl_cfg.copy_h = copy_h;
    lvgl_cfg.color = 0xFFFFFF;
    lvgl_cfg.opa = opa;

    // CLOGD("func: %s ", __FUNCTION__);

    ret = rgb565_dma2d_config(&lvgl_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
    // CHECK_RET_EQ(ret, SUCCESS);
error:

    return ret;
}


//  int32_t test_lv_rgb565_copy(void)
// {
//     int32_t ret = FAILURE;
//     uint8_t *src_buf = NULL;
//     uint8_t *dts_buf = NULL;
//     uint32_t src_offset = 0;
//     uint32_t dts_offset = 0;
//     uint32_t src_size_byte = 0;
//     uint32_t dts_size_byte = 0;

//     /* RGB565  src:320x240 -> dts:100x100 */
//     uint16_t src_img_width = 320;
//     uint16_t src_img_height = 240;
//     uint16_t src_start_width = 60;
//     uint16_t src_start_height = 20;
//     uint16_t dts_img_width = 640;
//     uint16_t dts_img_height = 172;
//     uint16_t dts_start_width = 240;
//     uint16_t dts_start_height = 32;
//     uint16_t copy_width = 100;
//     uint16_t copy_height = 100;

//     src_size_byte = src_img_width * src_img_height * RGB565_PIXEL_BYTE;
//     dts_size_byte = dts_img_width * dts_img_height * RGB565_PIXEL_BYTE;
//     src_offset = (src_img_width * src_start_height + src_start_width) * RGB565_PIXEL_BYTE;
//     dts_offset = (dts_img_width * dts_start_height + dts_start_width) * RGB565_PIXEL_BYTE;
//     CLOG("[%s:%d] src_size_byte=0x%x", __func__, __LINE__, src_size_byte);
//     CLOG("[%s:%d] dts_size_byte=0x%x", __func__, __LINE__, dts_size_byte);

//     /* malloc */
//     src_buf = tiny_malloc(src_size_byte);
//     CHECK_POINT_NOT_NULL_EXIT(src_buf, error);
//     CLOG("src_buf=0x%08x size=0x%x byte", src_buf, src_size_byte);

//     dts_buf = tiny_malloc(dts_size_byte);
//     CHECK_POINT_NOT_NULL_EXIT(dts_buf, error);
//     CLOG("dts_buf=0x%08x size=0x%x byte", dts_buf, dts_size_byte);

//     rgb565_colorbar_create((uint16_t *)src_buf, src_img_width, src_img_height, 40);
//     rgb565_line_color((uint16_t *)dts_buf, 0, dts_img_width, 0, dts_img_height, dts_img_width, RGB565_BRED);
//     CLOG("[%s:%d]", __func__, __LINE__);

//     ret = rgb565_dma2d_init();
//     CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
//     CLOG("[%s:%d]", __func__, __LINE__);

//     ret = lv_gpu_dma2d_copy(dts_buf + dts_offset, dts_img_width, src_buf + src_offset, src_img_width, copy_width, copy_height);
//     CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
//     CLOG("[%s:%d]", __func__, __LINE__);
//     //while(1);

//     ret = SUCCESS;

// error:
//     tiny_free(src_buf);
//     tiny_free(dts_buf);

//     if(ret == SUCCESS) {
//         CLOG("[%s:%d] test SUCCESS", __func__, __LINE__);
//     } else {
//         CLOG("[%s:%d] test FAILED", __func__, __LINE__);
//     }
//     return ret;
// }


//  int32_t test_lv_rgb565_fill(void)
// {
//     int32_t ret = FAILURE;
//     uint8_t *img_buf = NULL;
//     uint32_t img_offset = 0;
//     uint32_t img_size_byte = 0;

//     /* RGB565  src:320x240 -> dts:100x100 */
//     uint16_t img_width = 640;
//     uint16_t img_height = 172;
//     uint16_t start_width = 60;
//     uint16_t start_height = 20;
//     uint16_t color_width = 100;
//     uint16_t color_height = 10;
//     uint32_t color = RGB565_RED;      // 0xF800

//     img_size_byte = img_width * img_height * RGB565_PIXEL_BYTE;
//     img_offset = (img_width * start_height + start_width) * RGB565_PIXEL_BYTE;
//     CLOG("[%s:%d] img_size_byte=0x%x", __func__, __LINE__, img_size_byte);

//     /* malloc */
//     img_buf = tiny_malloc(img_size_byte);
//     CHECK_POINT_NOT_NULL_EXIT(img_buf, error);
//     CLOG("img_buf=0x%08x size=0x%x byte", img_buf, img_size_byte);

//     rgb565_colorbar_create((uint16_t *)img_buf, img_width, img_height, 40);
//     CLOG("[%s:%d]", __func__, __LINE__);

//     ret = rgb565_dma2d_init();
//     CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
//     CLOG("[%s:%d]", __func__, __LINE__);

//     ret = lv_gpu_dma2d_fill(img_buf + img_offset, img_width, color, color_width, color_height);
//     CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
//     CLOG("[%s:%d]", __func__, __LINE__);
//     //while(1);

//     ret = SUCCESS;

// error:
//     tiny_free(img_buf);

//     if(ret == SUCCESS) {
//         CLOG("[%s:%d] test SUCCESS", __func__, __LINE__);
//     } else {
//         CLOG("[%s:%d] test FAILED", __func__, __LINE__);
//     }
//     return ret;
// }


//  int32_t test_lv_rgb565_fill_mask(void)
// {
//     int32_t ret = FAILURE;
//     uint8_t *img_buf = NULL;
//     uint8_t *mask_buf = NULL;
//     uint32_t img_offset = 0;
//     uint32_t img_size_byte = 0;
//     uint32_t mask_size_byte = 0;
//     uint8_t mask_value = 0;
//     uint16_t y = 0;

//     /* RGB565  src:320x240 -> dts:100x100 */
//     uint16_t img_width = 640;
//     uint16_t img_height = 172;
//     uint16_t start_width = 60;
//     uint16_t start_height = 20;
//     uint16_t color_width = 100;
//     uint16_t color_height = 50;
//     uint32_t color = RGB565_RED;      // 0xF800
//     uint8_t opa = 0x80;

//     img_size_byte = img_width * img_height * RGB565_PIXEL_BYTE;
//     img_offset = (img_width * start_height + start_width) * RGB565_PIXEL_BYTE;
//     mask_size_byte = color_width * color_height;
//     CLOG("[%s:%d] img_size_byte=0x%x", __func__, __LINE__, img_size_byte);
//     CLOG("[%s:%d] mask_size_byte=0x%x", __func__, __LINE__, mask_size_byte);

//     /* malloc */
//     img_buf = tiny_malloc(img_size_byte);
//     CHECK_POINT_NOT_NULL_EXIT(img_buf, error);
//     CLOG("img_buf=0x%08x size=0x%x byte", img_buf, img_size_byte);

//     mask_buf = tiny_malloc(mask_size_byte);
//     CHECK_POINT_NOT_NULL_EXIT(mask_buf, error);
//     CLOG("mask_buf=0x%08x size=0x%x byte", mask_buf, mask_size_byte);

//     for(y = 0; y < color_height; y++)
//     {
//         mask_value = y << 4;
//         memset(mask_buf + (color_width * y), mask_value, color_width);
//     }
//     rgb565_colorbar_create((uint16_t *)img_buf, img_width, img_height, 40);
//     CLOG("[%s:%d]", __func__, __LINE__);

//     ret = rgb565_dma2d_init();
//     CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
//     CLOG("[%s:%d]", __func__, __LINE__);

//     ret = lv_gpu_dma2d_fill_mask(img_buf + img_offset, img_width, color, mask_buf, opa, color_width, color_height);
//     CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
//     CLOG("[%s:%d]", __func__, __LINE__);
//     //while(1);

//     ret = SUCCESS;

// error:
//     tiny_free(img_buf);
//     tiny_free(mask_buf);

//     if(ret == SUCCESS) {
//         CLOG("[%s:%d] test SUCCESS", __func__, __LINE__);
//     } else {
//         CLOG("[%s:%d] test FAILED", __func__, __LINE__);
//     }
//     return ret;
// }


//  int32_t test_lv_rgb565_blend(void)
// {
//     int32_t ret = FAILURE;
//     uint8_t *src_buf = NULL;
//     uint8_t *dts_buf = NULL;
//     uint32_t src_offset = 0;
//     uint32_t dts_offset = 0;
//     uint32_t src_size_byte = 0;
//     uint32_t dts_size_byte = 0;

//     /* RGB565  src:320x240 -> dts:100x100 */
//     uint16_t src_img_width = 320;
//     uint16_t src_img_height = 240;
//     uint16_t src_start_width = 60;
//     uint16_t src_start_height = 20;
//     uint16_t dts_img_width = 640;
//     uint16_t dts_img_height = 172;
//     uint16_t dts_start_width = 240;
//     uint16_t dts_start_height = 32;
//     uint16_t copy_width = 100;
//     uint16_t copy_height = 100;
//     uint8_t opa = 0x80;

//     src_size_byte = src_img_width * src_img_height * RGB565_PIXEL_BYTE;
//     dts_size_byte = dts_img_width * dts_img_height * RGB565_PIXEL_BYTE;
//     src_offset = (src_img_width * src_start_height + src_start_width) * RGB565_PIXEL_BYTE;
//     dts_offset = (dts_img_width * dts_start_height + dts_start_width) * RGB565_PIXEL_BYTE;
//     CLOG("[%s:%d] src_size_byte=0x%x", __func__, __LINE__, src_size_byte);
//     CLOG("[%s:%d] dts_size_byte=0x%x", __func__, __LINE__, dts_size_byte);

//     /* malloc */
//     src_buf = tiny_malloc(src_size_byte);
//     CHECK_POINT_NOT_NULL_EXIT(src_buf, error);
//     CLOG("src_buf=0x%08x size=0x%x byte", src_buf, src_size_byte);

//     dts_buf = tiny_malloc(dts_size_byte);
//     CHECK_POINT_NOT_NULL_EXIT(dts_buf, error);
//     CLOG("dts_buf=0x%08x size=0x%x byte", dts_buf, dts_size_byte);

//     rgb565_colorbar_create((uint16_t *)src_buf, src_img_width, src_img_height, 40);
//     rgb565_line_color((uint16_t *)dts_buf, 0, dts_img_width, 0, dts_img_height, dts_img_width, RGB565_BRED);
//     CLOG("[%s:%d]", __func__, __LINE__);

//     ret = rgb565_dma2d_init();
//     CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
//     CLOG("[%s:%d]", __func__, __LINE__);

//     ret = lv_gpu_dma2d_blend(dts_buf + dts_offset, dts_img_width, src_buf + src_offset, opa, src_img_width, copy_width, copy_height);
//     CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
//     CLOG("[%s:%d]", __func__, __LINE__);
//     //while(1);

//     ret = SUCCESS;

// error:
//     tiny_free(src_buf);
//     tiny_free(dts_buf);

//     if(ret == SUCCESS) {
//         CLOG("[%s:%d] test SUCCESS", __func__, __LINE__);
//     } else {
//         CLOG("[%s:%d] test FAILED", __func__, __LINE__);
//     }
//     return ret;
// }

#endif

