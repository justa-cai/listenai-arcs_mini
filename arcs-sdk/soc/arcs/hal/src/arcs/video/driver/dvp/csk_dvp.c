#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "chip.h"
#include "mmio.h"
#include "IOMuxManager.h"
#include "ClockManager.h"
#include "Driver_GPDMA.h"
#include "Driver_DVP.h"
#include "log_print.h"
#include "csk_timer.h"
#include "csk_clk_reset.h"
#include "check.h"
#include "csk_dvp.h"


static volatile uint32_t dvp_frm_cnt = 0;

void dvp_callback(DVP_emIrqEvent event, uint32_t param)
{
    void *dvp_dev = DVP0();
    uint32_t error;

    switch(event)
    {
        case DVP_IRQ_EVENT_SOF:
            //VIDEO_LOG("[%s:%d] DVP SOF event: %d", __func__, __LINE__, event);
            break;

        case DVP_IRQ_EVENT_EOF:
            //VIDEO_LOG("[%s:%d] DVP EOF event: %d", __func__, __LINE__, event);
            dvp_frm_cnt++;
            break;

#if 0
            if(dvp_frm_cnt >= DVP_TEST_FRAME_CNT_MAX)
            {
                /* turn off the clock out when the required frames received*/
            	DVP_DisableClockout(dvp_dev);
                DVP_Stop(dvp_dev);
            }
#endif
            //DVP_DisableClockout(dvp_dev);
            //DVP_Stop(dvp_dev);
            //DVP_Start(dvp_dev, &buffer_dvp);

            break;

        case DVP_IRQ_EVENT_EOF_CNT_ABNOR:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_EOF_CNT_ABNOR", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_DMA_VIC_SINGLE:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_DMA_VIC_SINGLE", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_DMA_VIC_REQ:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_DMA_VIC_REQ", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_FIFO_UNFLOW:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FIFO_UNFLOW", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_FIFO_OVFLOW:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FIFO_OVFLOW", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_FIFO_RD_EMPTY:
            //VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FIFO_RD_EMPTY", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_FIFO_RD_FULL:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FIFO_RD_FULL", __func__, __LINE__);
            break;

        default:
            //VIDEO_LOG("[%s:%d] DVP error event: %d", __func__, __LINE__, event);
            break;
    }

    error = DVP_GetError(dvp_dev, &error);

    if(error != DVP_ERROR_NONE)
    {
        /* turn off the clock out when the required frames received*/
        DVP_DisableClockout(dvp_dev);
        DVP_Stop(dvp_dev);
        VIDEO_LOG("[%s:%d] DVP Error code: %d", __func__, __LINE__, error);
    }

    return;
}


uint32_t dvp_frame_get(void)
{
    return dvp_frm_cnt;
}


int32_t dvp_init(DVP_InitTypeDef *dvp_cfg, uint32_t clk_hz)
{
    int32_t ret = 0;
    void *dvp_dev = DVP0();

    ret = DVP_Uninitialize(dvp_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DVP_DisableClockout(dvp_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DVP_Initialize(dvp_dev, dvp_callback, dvp_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DVP_EnableClockout(dvp_dev, clk_hz);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t dvp_deinit(void)
{
    int32_t ret = 0;
    void *dvp_dev = DVP0();

    ret = DVP_Uninitialize(dvp_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DVP_DisableClockout(dvp_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t dvp_start(void)
{
    int32_t ret = 0;
    void *dvp_dev = DVP0();

    ret = DVP_Start(dvp_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t dvp_stop(void)
{
    int32_t ret = 0;
    void *dvp_dev = DVP0();

    ret = DVP_Stop(dvp_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


void dvp_reg_dump(void)
{
    VIDEO_LOG("[DVP] F_HOR          *0x%08x = 0x%08x", &IP_DVP->REG_F_HOR.all, IP_DVP->REG_F_HOR.all);
    VIDEO_LOG("[DVP] F_VER          *0x%08x = 0x%08x", &IP_DVP->REG_F_VER.all, IP_DVP->REG_F_VER.all);
    VIDEO_LOG("[DVP] P_OFFSET       *0x%08x = 0x%08x", &IP_DVP->REG_P_OFFSET.all, IP_DVP->REG_P_OFFSET.all);
    VIDEO_LOG("[DVP] L_OFFSET       *0x%08x = 0x%08x", &IP_DVP->REG_L_OFFSET.all, IP_DVP->REG_L_OFFSET.all);
    VIDEO_LOG("[DVP] CLK_OUTEN      *0x%08x = 0x%08x", &IP_DVP->REG_CLK_OUTEN.all, IP_DVP->REG_CLK_OUTEN.all);
    VIDEO_LOG("[DVP] POL_CNTL       *0x%08x = 0x%08x", &IP_DVP->REG_POL_CNTL.all, IP_DVP->REG_POL_CNTL.all);
    VIDEO_LOG("[DVP] CLK_DIV        *0x%08x = 0x%08x", &IP_DVP->REG_JLB_HANSHK_SEL.all, IP_DVP->REG_JLB_HANSHK_SEL.all);
    VIDEO_LOG("[DVP] INPUT_FORM     *0x%08x = 0x%08x", &IP_DVP->REG_INPUT_FORM.all, IP_DVP->REG_INPUT_FORM.all);
    VIDEO_LOG("[DVP] VI_EN          *0x%08x = 0x%08x", &IP_DVP->REG_VI_EN.all, IP_DVP->REG_VI_EN.all);
    VIDEO_LOG("[DVP] DMA_BURST_THD  *0x%08x = 0x%08x", &IP_DVP->REG_DMA_BURST_THD.all, IP_DVP->REG_DMA_BURST_THD.all);
    VIDEO_LOG("[DVP] INTR_MSK       *0x%08x = 0x%08x", &IP_DVP->REG_INTR_MSK.all, IP_DVP->REG_INTR_MSK.all);
    VIDEO_LOG("[DVP] INTR_CLR       *0x%08x = 0x%08x", &IP_DVP->REG_INTR_CLR.all, IP_DVP->REG_INTR_CLR.all);
    VIDEO_LOG("[DVP] VIC_IRQ        *0x%08x = 0x%08x", &IP_DVP->REG_VIC_IRQ.all, IP_DVP->REG_VIC_IRQ.all);
    VIDEO_LOG("[DVP] VIC_INT_STATUS *0x%08x = 0x%08x", &IP_DVP->REG_VIC_INT_STATUS.all, IP_DVP->REG_VIC_INT_STATUS.all);
    VIDEO_LOG("[DVP] VIC_INT_RAW    *0x%08x = 0x%08x", &IP_DVP->REG_VIC_INT_RAW.all, IP_DVP->REG_VIC_INT_RAW.all);
}


void dvp_reset(void)
{
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x1;  // bit15
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIC_CLK = 0x1;    // bit21
    IP_AP_CFG->REG_DMA_SEL.bit.SEL_DVP_QSPI = 1;      // bit1  0:qspi_in  1:dvp
    IP_AP_CFG->REG_SW_RESET.bit.VIC_RESET = 1;        // bit9
}



/****************************************** GPDMA ************************************************/
static volatile uint32_t test_dvp_gpdma_finish_cnt = 0;

static void dvp_gpdma_callback(uint32_t event, void *workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    test_dvp_gpdma_finish_cnt++;
}


uint32_t dvp_gpdma_frame_get(void)
{
    return test_dvp_gpdma_finish_cnt;
}


int32_t dvp_gpdma_init(csk_gpdma_ch_t gpdma_ch, bool auto_en)
{
    int32_t ret;
    csk_gpdma_init_t gpdma_cfg = {
            .dma_ch = gpdma_ch,
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

    if(TRUE == auto_en) {
        gpdma_cfg.src_mode = address_mode_pipo;
        gpdma_cfg.dst_mode = address_mode_pipo;
    }

//    ret = GPDMA_Uninitialize();
//    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&gpdma_cfg, dvp_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return CSK_DRIVER_OK;
}


int32_t dvp_gpdma_start(csk_gpdma_ch_t gpdma_ch, void *pdst, uint32_t size_word, bool auto_en)
{
    int32_t ret;

    CHECK_POINT_NOT_NULL(pdst);

    //IP_GPDMA->REG_DMA_CH_CLR.bit.CFG_CH_CLR = 0x1 << gpdma_ch;

    if(TRUE == auto_en) {
        ret = GPDMA_Start_PiPo(gpdma_ch, (void*)DVP0_Buf(), (void*)DVP0_Buf(), pdst, pdst, size_word);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    } else {
        ret = GPDMA_Start_Normal(gpdma_ch, (void*)DVP0_Buf(), pdst, size_word);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }

    return ret;
}


void gpdma_reg_dump(uint8_t dma_ch)
{
    VIDEO_LOG("DMA_CH%d_CTRL       *0x%x=0x%x", dma_ch, (GPDMA_BASE + 0x00 + 0x04 * dma_ch), mmio_read32(GPDMA_BASE + 0x00 + 0x04 * dma_ch));
    VIDEO_LOG("DMA_BLOCK_LEN_CH%d  *0x%x=0x%x", dma_ch, (GPDMA_BASE + 0x2C + 0x04 * dma_ch), mmio_read32(GPDMA_BASE + 0x2C + 0x04 * dma_ch));
    if(dma_ch < gp_dma_ch5) {
        VIDEO_LOG("DMA_SRC_ADDR0_CH%d  *0x%x=0x%x", dma_ch, (GPDMA_BASE + 0x54 + 0x10 * dma_ch), mmio_read32(GPDMA_BASE + 0x54 + 0x10 * dma_ch));
        VIDEO_LOG("DMA_SRC_ADDR1_CH%d  *0x%x=0x%x", dma_ch, (GPDMA_BASE + 0x58 + 0x10 * dma_ch), mmio_read32(GPDMA_BASE + 0x58 + 0x10 * dma_ch));
        VIDEO_LOG("DMA_DST_ADDR0_CH%d  *0x%x=0x%x", dma_ch, (GPDMA_BASE + 0x5C + 0x10 * dma_ch), mmio_read32(GPDMA_BASE + 0x5C + 0x10 * dma_ch));
        if (gp_dma_ch4 == dma_ch)
            VIDEO_LOG("DMA_DST_ADDR1_CH%d  *0x%x=0x%x", dma_ch, (GPDMA_BASE + 0x100), mmio_read32(GPDMA_BASE + 0x100));
        else
            VIDEO_LOG("DMA_DST_ADDR1_CH%d  *0x%x=0x%x", dma_ch, (GPDMA_BASE + 0x60 + 0x10 * dma_ch), mmio_read32(GPDMA_BASE + 0x60 + 0x10 * dma_ch));
    } else {
        VIDEO_LOG("DMA_SRC_ADDR0_CH%d  *0x%x=0x%x", dma_ch, (GPDMA_BASE + 0x104 + 0x10 * (dma_ch-gp_dma_ch5)), mmio_read32(GPDMA_BASE + 0x104 + 0x10 * (dma_ch-gp_dma_ch5)));
        VIDEO_LOG("DMA_SRC_ADDR1_CH%d  *0x%x=0x%x", dma_ch, (GPDMA_BASE + 0x108 + 0x10 * (dma_ch-gp_dma_ch5)), mmio_read32(GPDMA_BASE + 0x108 + 0x10 * (dma_ch-gp_dma_ch5)));
        VIDEO_LOG("DMA_DST_ADDR0_CH%d  *0x%x=0x%x", dma_ch, (GPDMA_BASE + 0x10C + 0x10 * (dma_ch-gp_dma_ch5)), mmio_read32(GPDMA_BASE + 0x10C + 0x10 * (dma_ch-gp_dma_ch5)));
        VIDEO_LOG("DMA_DST_ADDR1_CH%d  *0x%x=0x%x", dma_ch, (GPDMA_BASE + 0x110 + 0x10 * (dma_ch-gp_dma_ch5)), mmio_read32(GPDMA_BASE + 0x110 + 0x10 * (dma_ch-gp_dma_ch5)));
    }

    if (dma_ch <= gp_dma_ch5) {
        VIDEO_LOG("DMA_INT_STATUS      *0x%x=0x%x", (GPDMA_BASE + 0x158), mmio_read32(GPDMA_BASE + 0x158));
    } else {
        VIDEO_LOG("DMA_INT_STATUS	  *0x%x=0x%x", (GPDMA_BASE + 0x2AC), mmio_read32(GPDMA_BASE + 0x2AC));
    }
}



