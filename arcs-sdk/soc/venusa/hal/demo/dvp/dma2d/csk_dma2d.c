#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "queue.h"
#include "list.h"

#include "venusa_ap.h"
#include "log_print.h"
#include "Driver_DMA2D.h"
#include "csk_dma2d.h"
#include "check.h"

static SemaphoreHandle_t dma2dSemaphore = NULL;      // DMA2D done

static void csk_dma2d_callback(uint32_t event, void *workspace)
{
    //CLOG("[%s:%d] event=0x%x", __func__, __LINE__, event);

    if(event & CSK_DMA2D_EVENT_TRANSFER_DONE)
    {
        if (dma2dSemaphore != NULL) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(dma2dSemaphore, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}


int32_t dma2d_scaler_start(SemaphoreHandle_t Semaphore, csk_dma2d_scaler_crop_t *cfg)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t img_out_size = 0;
    csk_dma2d_init_t dma2d_para;
    csk_dma_2d_image_cfg_t dma2d_img_cfg;

    //CLOG("[%s:%d]", __FUNCTION__, __LINE__);

    if ((NULL == cfg) || (NULL == cfg->img_buf_in) || (NULL == cfg->img_buf_out))
    {
        CLOGE("[%s:%d] buffer is NULL", __FUNCTION__, __LINE__);
        return -1;
    }

    dma2dSemaphore = Semaphore;

    memset(&dma2d_para, 0, sizeof(dma2d_para));
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_para.dma_ch = dma_2d_ch0;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;

    dma2d_img_cfg.img_input.img_width = cfg->img_width_in;
    dma2d_img_cfg.img_input.img_height = cfg->img_height_in;
    dma2d_img_cfg.img_input.img_format = csk_image_format_yuv422_yuyv_packed;
    dma2d_img_cfg.img_input.img_line_stride = dma2d_img_cfg.img_input.img_width * 2;  // YUV422
    dma2d_img_cfg.img_output.img_width = cfg->img_width_out;
    dma2d_img_cfg.img_output.img_height = cfg->img_height_out;
    dma2d_img_cfg.img_output.img_format = csk_image_format_yuv422_yuyv_packed;
    dma2d_img_cfg.img_output.img_line_stride = dma2d_img_cfg.img_output.img_width * 2;  // YUV422
    dma2d_img_cfg.img_scaler_en = csk_func_enable;

    img_out_size = cfg->img_width_out * cfg->img_height_out * 2;  // YUV422

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, csk_dma2d_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(dma2d_para.dma_ch, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(dma2d_para.dma_ch, cfg->img_buf_in, cfg->img_buf_out, img_out_size);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t dma2d_crop_start(SemaphoreHandle_t Semaphore, csk_dma2d_scaler_crop_t *cfg)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t img_out_size = 0;
    uint32_t img_buff_offset = 0;
    csk_dma2d_init_t dma2d_para;
    csk_dma_2d_image_cfg_t dma2d_img_cfg;

    //CLOG("[%s:%d]", __FUNCTION__, __LINE__);

    if ((NULL == cfg) || (NULL == cfg->img_buf_in) || (NULL == cfg->img_buf_out))
    {
        CLOGE("[%s:%d] buffer is NULL", __FUNCTION__, __LINE__);
        return -1;
    }

    dma2dSemaphore = Semaphore;

    memset(&dma2d_para, 0, sizeof(dma2d_para));
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_para.dma_ch = dma_2d_ch0;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;

    dma2d_img_cfg.img_input.img_width = cfg->img_width_out;
    dma2d_img_cfg.img_input.img_height = cfg->img_height_out;
    dma2d_img_cfg.img_input.img_format = csk_image_format_yuv422_yuyv_packed;
    dma2d_img_cfg.img_input.img_line_stride = cfg->img_width_in * 2;        // YUV422
    dma2d_img_cfg.img_output.img_width = cfg->img_width_out;
    dma2d_img_cfg.img_output.img_height = cfg->img_height_out;
    dma2d_img_cfg.img_output.img_format = csk_image_format_yuv422_yuyv_packed;
    dma2d_img_cfg.img_output.img_line_stride = cfg->img_width_out * 2;      // YUV422
    dma2d_img_cfg.img_crop_en = csk_func_enable;

    img_out_size = cfg->img_width_in * cfg->img_height_in * 2;              // YUV422
    img_buff_offset = (cfg->crop_y * cfg->img_width_in + cfg->crop_x) * 2;  // YUV422

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, csk_dma2d_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(dma2d_para.dma_ch, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Start_Normal(dma2d_para.dma_ch, cfg->img_buf_in + img_buff_offset, cfg->img_buf_out, img_out_size);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

