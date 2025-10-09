#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "chip.h"
#include "mmio.h"
#include "IOMuxManager.h"
#include "ClockManager.h"
#include "Driver_GPDMA.h"
#include "Driver_RGB.h"
#include "log_print.h"
#include "csk_timer.h"
#include "csk_clk_reset.h"
#include "check.h"
#include "csk_rgb.h"


static RGB_irq_cnt_t rgb_irq_cnt = {0};

void rgb_callback(RGB_emIrqEvent event, uint32_t param)
{
    void *rgb_dev = RGB0();
    uint32_t error;

    switch(event)
    {
        case RGB_IRQ_EVENT_SOF:
            rgb_irq_cnt.sof++;
            //VIDEO_LOG("[%s:%d] RGB SOF event: %d", __func__, __LINE__, event);
            break;

        case RGB_IRQ_EVENT_EOF:
            rgb_irq_cnt.eof++;
            //VIDEO_LOG("[%s:%d] RGB EOF event: %d", __func__, __LINE__, event);
            break;

        case RGB_IRQ_EVENT_FIFO_RD_EMPTY:
            rgb_irq_cnt.fifo_read_empty++;
            VIDEO_LOG("[%s:%d] RGB_IRQ_EVENT_FIFO_RD_EMPTY", __func__, __LINE__);
            break;

        case RGB_IRQ_EVENT_FIFO_RD_FULL:
            rgb_irq_cnt.fifo_read_full++;
            VIDEO_LOG("[%s:%d] RGB_IRQ_EVENT_FIFO_RD_FULL", __func__, __LINE__);
            break;

        case RGB_IRQ_EVENT_FIFO_WR_EMPTY:
            rgb_irq_cnt.fifo_write_empty++;
            VIDEO_LOG("[%s:%d] RGB_IRQ_EVENT_FIFO_WR_EMPTY", __func__, __LINE__);
            break;

        case RGB_IRQ_EVENT_FIFO_WR_FULL:
            rgb_irq_cnt.fifo_write_empty++;
            VIDEO_LOG("[%s:%d] RGB_IRQ_EVENT_FIFO_WR_FULL", __func__, __LINE__);
            break;

        default:
            //VIDEO_LOG("[%s:%d] RGB error event: %d", __func__, __LINE__, event);
            break;
    }

    error = RGB_GetError(rgb_dev, &error);

    if(error != RGB_ERROR_NONE)
    {
        /* turn off the clock out when the required frames received*/
        RGB_DisableClockout();
        RGB_Stop(rgb_dev);
        VIDEO_LOG("[%s:%d] RGB Error code: %d", __func__, __LINE__, error);
    }

    return;
}


RGB_irq_cnt_t* rgb_irq_cnt_get(void)
{
    return &rgb_irq_cnt;
}


int32_t rgb_init(RGB_InitTypeDef *prgb_cfg)
{
    int32_t ret = 0;
    void *rgb_dev = RGB0();

    ret = RGB_Uninitialize(rgb_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = RGB_DisableClockout();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = RGB_Initialize(rgb_dev, rgb_callback, prgb_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = RGB_EnableClockout(prgb_cfg->clk_hz);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t rgb_deinit(void)
{
    int32_t ret = 0;
    void *rgb_dev = RGB0();

    ret = RGB_Uninitialize(rgb_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = RGB_DisableClockout();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t rgb_start(void)
{
    int32_t ret = 0;
    void *rgb_dev = RGB0();

    ret = RGB_Start(rgb_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t rgb_stop(void)
{
    int32_t ret = 0;
    void *rgb_dev = RGB0();

    ret = RGB_Stop(rgb_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


void rgb_reg_dump(void)
{
    VIDEO_LOG("0x00 RGB_CONTROL0         *0x%08x=0x%08x", &IP_RGB->REG_RGB_CONTROL0.all,        IP_RGB->REG_RGB_CONTROL0.all       );
    VIDEO_LOG("0x04 RGB_CONTROL1         *0x%08x=0x%08x", &IP_RGB->REG_RGB_CONTROL1.all,        IP_RGB->REG_RGB_CONTROL1.all       );
    VIDEO_LOG("0x08 SEQUENTIAL_CONTROL0  *0x%08x=0x%08x", &IP_RGB->REG_SEQUENTIAL_CONTROL0.all, IP_RGB->REG_SEQUENTIAL_CONTROL0.all);
    VIDEO_LOG("0x0C SEQUENTIAL_CONTROL1  *0x%08x=0x%08x", &IP_RGB->REG_SEQUENTIAL_CONTROL1.all, IP_RGB->REG_SEQUENTIAL_CONTROL1.all);
    VIDEO_LOG("0x10 IMAGE_SIZE           *0x%08x=0x%08x", &IP_RGB->REG_IMAGE_SIZE.all,          IP_RGB->REG_IMAGE_SIZE.all         );
    VIDEO_LOG("0x14 RGB_IRQ              *0x%08x=0x%08x", &IP_RGB->REG_RGB_IRQ.all,             IP_RGB->REG_RGB_IRQ.all            );
    VIDEO_LOG("0x18 RGB_INTR_MSK         *0x%08x=0x%08x", &IP_RGB->REG_RGB_INTR_MSK.all,        IP_RGB->REG_RGB_INTR_MSK.all       );
    VIDEO_LOG("0x1C RGB_INTR_CLR         *0x%08x=0x%08x", &IP_RGB->REG_RGB_INTR_CLR.all,        IP_RGB->REG_RGB_INTR_CLR.all       );
    VIDEO_LOG("0x20 RGB_INTR_STATUS      *0x%08x=0x%08x", &IP_RGB->REG_RGB_INTR_STATUS.all,     IP_RGB->REG_RGB_INTR_STATUS.all    );
    VIDEO_LOG("0x24 RGB_INTR_RAW         *0x%08x=0x%08x", &IP_RGB->REG_RGB_INTR_RAW.all,        IP_RGB->REG_RGB_INTR_RAW.all       );
}


void rgb_reset(void)
{
    ap_cfg_gpdma_clk_enable();
    ap_cfg_video_clk_enable();
    ap_cfg_rgb_clk_enable();
    ap_cfg_rgb_clk_sel_24();
    ap_cfg_rgb_clk_div_m(1);
    //ap_cfg_rgb_clk_inv();
    ap_cfg_rgb_clk_ld();
    ap_cfg_rgb_reset();
    ap_cfg_gpdma_reset();
    ap_cfg_dma_sel_rgb();
    //ap_cfg_reg_dump();
}



/****************************************** GPDMA ************************************************/
static volatile uint32_t rgb_gpdma_finish_cnt = 0;

static void rgb_gpdma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    rgb_gpdma_finish_cnt++;
}

int32_t rgb_gpdma_init(csk_gpdma_ch_t gpdma_ch)
{
    int32_t ret;

    csk_gpdma_init_t gpdma_cfg = {
            .dma_ch = gpdma_ch,
            .burst_len = gpdma_burst_len_8spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_m2p,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_fix,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = qspi_hs_num0,
    };

    ret = GPDMA_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&gpdma_cfg, rgb_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return CSK_DRIVER_OK;
}


int32_t rgb_gpdma_start(csk_gpdma_ch_t gpdma_ch, void* pbuf, uint32_t size_word)
{
    int32_t ret;

    CHECK_POINT_NOT_NULL(pbuf);

    ret = GPDMA_Start_PiPo(gpdma_ch, pbuf, pbuf, (void*)RGB0_Buf(), (void*)RGB0_Buf(), size_word);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

int32_t rgb_gpdma_stop(csk_gpdma_ch_t gpdma_ch)
{
    int32_t ret;

    ret = GPDMA_Stop(gpdma_ch);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

uint32_t rgb_gpdma_get_cnt(void)
{
    return rgb_gpdma_finish_cnt;
}


