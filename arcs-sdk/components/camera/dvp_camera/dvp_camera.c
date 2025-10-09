#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "portmacro.h"
#include "projdefs.h"
#include "queue.h"

#include "IOMuxManager.h"
#include "Driver_GPDMA.h"
#include "Driver_DVP.h"

#include "camera_xfer.h"
#include "lisa_log.h"
// #include "arcs_ap.h"

static void *dvp_dev = NULL;
static uint8_t dvp_gpdma_ch = 0;
static struct cam_xfer_queue *cam_data_queue = NULL;
volatile static struct cam_ipeg_mem *cam_img_mem = NULL;

#if 0
void dvp_reg_dump(void)
{
    LOGI("[DVP] F_HOR          *0x%08x = 0x%08x", &IP_DVP->REG_F_HOR.all, IP_DVP->REG_F_HOR.all);
    LOGI("[DVP] F_VER          *0x%08x = 0x%08x", &IP_DVP->REG_F_VER.all, IP_DVP->REG_F_VER.all);
    LOGI("[DVP] P_OFFSET       *0x%08x = 0x%08x", &IP_DVP->REG_P_OFFSET.all, IP_DVP->REG_P_OFFSET.all);
    LOGI("[DVP] L_OFFSET       *0x%08x = 0x%08x", &IP_DVP->REG_L_OFFSET.all, IP_DVP->REG_L_OFFSET.all);
    LOGI("[DVP] CLK_OUTEN      *0x%08x = 0x%08x", &IP_DVP->REG_CLK_OUTEN.all, IP_DVP->REG_CLK_OUTEN.all);
    LOGI("[DVP] POL_CNTL       *0x%08x = 0x%08x", &IP_DVP->REG_POL_CNTL.all, IP_DVP->REG_POL_CNTL.all);
    LOGI("[DVP] CLK_DIV        *0x%08x = 0x%08x", &IP_DVP->REG_JLB_HANSHK_SEL.all, IP_DVP->REG_JLB_HANSHK_SEL.all);
    LOGI("[DVP] INPUT_FORM     *0x%08x = 0x%08x", &IP_DVP->REG_INPUT_FORM.all, IP_DVP->REG_INPUT_FORM.all);
    LOGI("[DVP] VI_EN          *0x%08x = 0x%08x", &IP_DVP->REG_VI_EN.all, IP_DVP->REG_VI_EN.all);
    LOGI("[DVP] DMA_BURST_THD  *0x%08x = 0x%08x", &IP_DVP->REG_DMA_BURST_THD.all, IP_DVP->REG_DMA_BURST_THD.all);
    LOGI("[DVP] INTR_MSK       *0x%08x = 0x%08x", &IP_DVP->REG_INTR_MSK.all, IP_DVP->REG_INTR_MSK.all);
    LOGI("[DVP] INTR_CLR       *0x%08x = 0x%08x", &IP_DVP->REG_INTR_CLR.all, IP_DVP->REG_INTR_CLR.all);
    LOGI("[DVP] VIC_IRQ        *0x%08x = 0x%08x", &IP_DVP->REG_VIC_IRQ.all, IP_DVP->REG_VIC_IRQ.all);
    LOGI("[DVP] VIC_INT_STATUS *0x%08x = 0x%08x", &IP_DVP->REG_VIC_INT_STATUS.all, IP_DVP->REG_VIC_INT_STATUS.all);
    LOGI("[DVP] VIC_INT_RAW    *0x%08x = 0x%08x", &IP_DVP->REG_VIC_INT_RAW.all, IP_DVP->REG_VIC_INT_RAW.all);
}
#endif

static void dvp_event_callback(DVP_emIrqEvent event, uint32_t param)
{
    void *dvp_dev = DVP0();
    uint32_t error;

    switch(event) {
        case DVP_IRQ_EVENT_SOF:
            // Handle Start of Frame event
            break;

        case DVP_IRQ_EVENT_EOF:
            // Handle End of Frame event
            break;

        case DVP_IRQ_EVENT_EOF_CNT_ABNOR:
            // Handle abnormal EOF count event
            break;

        case DVP_IRQ_EVENT_DMA_VIC_SINGLE:
            // Handle single DMA VIC request event
            break;

        case DVP_IRQ_EVENT_DMA_VIC_REQ:
            // Handle DMA VIC request event
            break;

        case DVP_IRQ_EVENT_FIFO_OVFLOW:
            // Handle FIFO overflow event
            break;

        case DVP_IRQ_EVENT_FIFO_UNFLOW:
            // Handle FIFO underflow event
            break;

        case DVP_IRQ_EVENT_FIFO_RD_EMPTY:
            // Handle FIFO read empty event
            break;

        case DVP_IRQ_EVENT_FIFO_RD_FULL:
            // Handle FIFO read full event
            break;

        default:
            // Handle unknown event
            break;
    }

    error = DVP_GetError(dvp_dev, &error);
    if(error != DVP_ERROR_NONE) {
        /* turn off the clock out when the required frames received*/
        DVP_DisableClockout(dvp_dev);
        DVP_Stop(dvp_dev);
        LOGE("DVP Error code: %d", error);
    }
}

static void dvp_gpdma_event_callback(uint32_t event, void *workspace)
{
    struct cam_xfer_queue *queue = (struct cam_xfer_queue *)workspace;
    struct cam_ipeg_mem *tmp_mem;
    
    BaseType_t yield = pdFALSE;
    if (xQueueReceiveFromISR(queue->queue_in, &tmp_mem, &yield) != pdTRUE) {
        LOGE("xQueueReceiveFromISR failed\n");
    }
    else{
        cam_img_mem->buf.readed = cam_img_mem->buf.size;
        if (xQueueSendFromISR(queue->queue_out, &cam_img_mem, &yield) != pdTRUE) {
            LOGE("xQueueSendFromISR failed\n");
        }
        cam_img_mem = tmp_mem;
    }

    if (CSK_DRIVER_OK != GPDMA_Start_Normal(dvp_gpdma_ch, (void *)DVP0_Buf(), cam_img_mem->buf.addr, cam_img_mem->buf.size / sizeof(uint32_t))) {
        LOGE("GPDMA_Start_Normal failed\n");
    }
}

static void dvp_camera_pinmux(const dvp_config_t *dvp_config)
{
    IOMuxManager_PinConfigure(dvp_config->pins.hsync.pad, dvp_config->pins.hsync.pin, dvp_config->pins.hsync.func);  // HSYNC
    IOMuxManager_PinConfigure(dvp_config->pins.vsync.pad, dvp_config->pins.vsync.pin, dvp_config->pins.vsync.func);  // VSYNC
    IOMuxManager_PinConfigure(dvp_config->pins.pclk.pad, dvp_config->pins.pclk.pin, dvp_config->pins.pclk.func);  // PCLK
    for (int i = 0; i < 8; i++) {
        IOMuxManager_PinConfigure(dvp_config->pins.data[i].pad, dvp_config->pins.data[i].pin, dvp_config->pins.data[i].func);
    }
}

int dvp_camera_init(const xfer_hw_config_t *config, struct cam_xfer_queue *queue)
{
    int32_t ret = 0;
    dvp_dev = config->dvp_config.dvp_dev;
    const dvp_config_t *dvp_config = &config->dvp_config;
    dvp_gpdma_ch = dvp_config->dma_channel;

    dvp_camera_pinmux(dvp_config);

    DVP_InitTypeDef dvp_cfg = {
        .FrameWidth  = queue->width,
        .FrameHeight = queue->height,
        .PixelOffset = dvp_config->pixel_offset,
        .LineOffset  = dvp_config->line_offset,
        .InputFormat = dvp_config->input_format,
        .PCKPolarity = dvp_config->pck_polarity,
        .VSPolarity  = dvp_config->vs_polarity,
        .HSPolarity  = dvp_config->hs_polarity,
        .DataAlign   = dvp_config->data_align,
    };
    ret = DVP_Initialize(dvp_dev, dvp_event_callback, &dvp_cfg);
    if (ret != CSK_DRIVER_OK) {
        printf("DVP_Initialize failed %d\n", ret);
        return -1;
    }

    ret = GPDMA_Initialize();
    if (ret != CSK_DRIVER_OK) {
        printf("GPDMA_Initialize failed %d\n", ret);
        return -1;
    }

    csk_gpdma_init_t dvp_gpdma_cfg = {
        .dma_ch = dvp_config->dma_channel,
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
    ret = GPDMA_Config(&dvp_gpdma_cfg, dvp_gpdma_event_callback, queue);
    if (ret != CSK_DRIVER_OK) {
        printf("GPDMA_Config failed %d\n", ret);
        return -1;
    }

    cam_data_queue = queue;

    return 0;
}

int dvp_camera_deinit(void)
{
    int32_t ret = 0;

    ret = DVP_Uninitialize(dvp_dev);
    if (ret != CSK_DRIVER_OK) {
        printf("DVP_Uninitialize failed %d\n", ret);
        return -1;
    }

    ret = GPDMA_Uninitialize();
    if (ret != CSK_DRIVER_OK) {
        printf("GPDMA_Uninitialize failed %d\n", ret);
        return -1;
    }

    return 0;
}

int dvp_camera_start(void)
{
    int32_t ret = 0;

    if (xQueueReceive(cam_data_queue->queue_in, &cam_img_mem, pdMS_TO_TICKS(100)) != pdTRUE) {
        LOGE("xQueueReceive failed\n");
        return -1;
    }
    // ret = GPDMA_Start_PiPo(DVP_CAMERA_GPDMA_CH, DVP0_Buf(), DVP0_Buf(), NULL, NULL, size_word);
    ret = GPDMA_Start_Normal(dvp_gpdma_ch, (void*)DVP0_Buf(), cam_img_mem->buf.addr, cam_img_mem->buf.size / sizeof(uint32_t));
    if (ret != CSK_DRIVER_OK) {
        LOGE("GPDMA_Start_Normal failed %d\n", ret);
        return -1;
    }

    ret = DVP_Start(dvp_dev);
    if (ret != CSK_DRIVER_OK) {
        LOGE("DVP_Start failed %d\n", ret);
        return -1;
    }

    return 0;
}

int dvp_camera_stop(void)
{
    int32_t ret = 0;

    ret = DVP_Stop(dvp_dev);
    if (ret != CSK_DRIVER_OK) {
        LOGE("DVP_Stop failed %d\n", ret);
        return -1;
    }

    ret = GPDMA_Stop(dvp_gpdma_ch);
    if (ret != CSK_DRIVER_OK) {
        LOGE("GPDMA_Stop failed %d\n", ret);
        return -1;
    }
    
    /*返还正在接收的buffer*/
    if(cam_img_mem != NULL){
        if (xQueueSend(cam_data_queue->queue_out, &cam_img_mem, 0) != pdTRUE) {
            LOGE("CameraxQueueSendFromISR failed\n");
        }
    }

    return 0;
}

static struct cam_xfer_ops dvp_camera_xfer_ops = {
    .cam_xfer_init = dvp_camera_init,
    .cam_xfer_deinit = dvp_camera_deinit,
    .cam_xfer_start = dvp_camera_start,
    .cam_xfer_stop = dvp_camera_stop,
};

static struct cam_xfer dvp_camera_xfer = {
    .ops = &dvp_camera_xfer_ops,
};

struct cam_xfer *spi_xfer_get(void)
{
    return &dvp_camera_xfer;
}
