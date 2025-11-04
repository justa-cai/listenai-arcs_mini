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
#include "Driver_QSPI_SENSOR_IN.h"
#include "log_print.h"
#include "csk_timer.h"
#include "csk_clk_reset.h"
#include "check.h"
#include "csk_qspi_in.h"


static volatile uint32_t qspi_in_sof_cnt = 0;
static volatile uint32_t qspi_in_eof_cnt = 0;

static void qspi_in_callback(em_QSPI_SENSOR_IN_IrqEvent event, uint32_t param)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);

    switch(event)
    {
        case QSPI_SENSOR_IN_IRQ_EVENT_FRAME_START:
            //VIDEO_LOG("[%s:%d] QSPI_IN SOF event: %d", __func__, __LINE__, event);
            qspi_in_sof_cnt++;
            break;

        case QSPI_SENSOR_IN_IRQ_EVENT_FRAME_END:
            //VIDEO_LOG("[%s:%d] QSPI_IN EOF event: %d", __func__, __LINE__, event);
            qspi_in_eof_cnt++;
            break;

        case QSPI_SENSOR_IN_IRQ_EVENT_RXFIFOOR:
            VIDEO_LOG("[%s:%d] QSPI_IN RX FIFO overflow event: %d", __func__, __LINE__, event);
            break;

        default:
            VIDEO_LOG("[%s:%d] QSPI_IN error event: %d", __func__, __LINE__, event);
            break;
    }
}


uint32_t qspi_in_sof_cnt_get(void)
{
    return qspi_in_sof_cnt;
}


uint32_t qspi_in_eof_cnt_get(void)
{
    return qspi_in_eof_cnt;
}


int32_t qspi_in_init(QSPI_SENSOR_IN_InitTypeDef *pcfg, uint32_t clk_hz)
{
    int32_t ret = 0;
    void *qspi_in_dev = QSPI_SENSOR_IN0();

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = QSPI_SENSOR_IN_Initialize(qspi_in_dev, qspi_in_callback, pcfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* MCLK out to camera */
    DVP_EnableClockout(DVP0(), clk_hz);

    return ret;
}


int32_t qspi_in_deinit(void)
{
    int32_t ret = 0;
    void *qspi_in_dev = QSPI_SENSOR_IN0();

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    DVP_DisableClockout(DVP0());

    ret = QSPI_SENSOR_IN_Uninitialize(qspi_in_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t qspi_in_start(void)
{
    int32_t ret = 0;
    void *qspi_in_dev = QSPI_SENSOR_IN0();

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = QSPI_SENSOR_IN_Start(qspi_in_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t qspi_in_stop(void)
{
    int32_t ret = 0;
    void *qspi_in_dev = QSPI_SENSOR_IN0();

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = QSPI_SENSOR_IN_Stop(qspi_in_dev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


void qspi_in_msg_dump(void)
{
    uint32_t value = 0;

    VIDEO_LOG("sof_cnt=%d", qspi_in_sof_cnt_get());
    VIDEO_LOG("eof_cnt=%d", qspi_in_eof_cnt_get());

    value = IP_QSPI_SENSOR_IN->REG_SPI_CAMERA_CTRL.all;
    VIDEO_LOG("data_id=0x%x", (value >> 8) & 0xFF);
    VIDEO_LOG("packet_size=%d", (value >> 16) & 0xFFFF);

    value = IP_QSPI_SENSOR_IN->REG_IMAGE_SIZE.all;
    VIDEO_LOG("iamge_width=%d", (value >> 16) & 0xFFFF);
    VIDEO_LOG("iamge_height=%d", value & 0xFFFF);

    value = IP_QSPI_SENSOR_IN->REG_LINE_NUM.all;
    VIDEO_LOG("line_num=%d", value & 0xFFFF);

    VIDEO_LOG("0x68 PACKET_ID  *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_PACKET_ID.all,        IP_QSPI_SENSOR_IN->REG_PACKET_ID.all);
    VIDEO_LOG("0x6C SYNC_CODE  *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_SYNC_CODE.all,        IP_QSPI_SENSOR_IN->REG_SYNC_CODE.all);
    VIDEO_LOG("0x70 CTRL       *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_SPI_CAMERA_CTRL.all,  IP_QSPI_SENSOR_IN->REG_SPI_CAMERA_CTRL.all);
    VIDEO_LOG("0x74 IMAGE_SIZE *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_IMAGE_SIZE.all,       IP_QSPI_SENSOR_IN->REG_IMAGE_SIZE.all);
    VIDEO_LOG("0x78 LINE_NUM   *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_LINE_NUM.all,         IP_QSPI_SENSOR_IN->REG_LINE_NUM.all);
    VIDEO_LOG("0x7C CONFIG     *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_CONFIG.all,           IP_QSPI_SENSOR_IN->REG_CONFIG.all);
}


void qspi_in_reg_dump(void)
{
    VIDEO_LOG("0x00 IDREV      *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_IDREV.all,            IP_QSPI_SENSOR_IN->REG_IDREV.all);
    VIDEO_LOG("0x10 TRANSFMT   *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_TRANSFMT.all,         IP_QSPI_SENSOR_IN->REG_TRANSFMT.all);
    VIDEO_LOG("0x14 DIRECTIO   *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_DIRECTIO.all,         IP_QSPI_SENSOR_IN->REG_DIRECTIO.all);
    VIDEO_LOG("0x20 TRANSCTRL  *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_TRANSCTRL.all,        IP_QSPI_SENSOR_IN->REG_TRANSCTRL.all);
    VIDEO_LOG("0x24 CMD        *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_CMD.all,              IP_QSPI_SENSOR_IN->REG_CMD.all);
    VIDEO_LOG("0x28 ADDR       *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_ADDR.all,             IP_QSPI_SENSOR_IN->REG_ADDR.all);
    VIDEO_LOG("0x2C DATA       *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_DATA.all,             IP_QSPI_SENSOR_IN->REG_DATA.all);
    VIDEO_LOG("0x30 CTRL       *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_CTRL.all,             IP_QSPI_SENSOR_IN->REG_CTRL.all);
    VIDEO_LOG("0x34 STATUS     *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_STATUS.all,           IP_QSPI_SENSOR_IN->REG_STATUS.all);
    VIDEO_LOG("0x38 INTREN     *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_INTREN.all,           IP_QSPI_SENSOR_IN->REG_INTREN.all);
    VIDEO_LOG("0x3C INTRST     *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_INTRST.all,           IP_QSPI_SENSOR_IN->REG_INTRST.all);
    VIDEO_LOG("0x40 TIMING     *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_TIMING.all,           IP_QSPI_SENSOR_IN->REG_TIMING.all);
    VIDEO_LOG("0x50 MEMCTRL    *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_MEMCTRL.all,          IP_QSPI_SENSOR_IN->REG_MEMCTRL.all);
    VIDEO_LOG("0x60 SLVST      *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_SLVST.all,            IP_QSPI_SENSOR_IN->REG_SLVST.all);
    VIDEO_LOG("0x64 SLVDATACNT *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_SLVDATACNT.all,       IP_QSPI_SENSOR_IN->REG_SLVDATACNT.all);
    VIDEO_LOG("0x68 PACKET_ID  *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_PACKET_ID.all,        IP_QSPI_SENSOR_IN->REG_PACKET_ID.all);
    VIDEO_LOG("0x6C SYNC_CODE  *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_SYNC_CODE.all,        IP_QSPI_SENSOR_IN->REG_SYNC_CODE.all);
    VIDEO_LOG("0x70 CTRL       *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_SPI_CAMERA_CTRL.all,  IP_QSPI_SENSOR_IN->REG_SPI_CAMERA_CTRL.all);
    VIDEO_LOG("0x74 IMAGE_SIZE *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_IMAGE_SIZE.all,       IP_QSPI_SENSOR_IN->REG_IMAGE_SIZE.all);
    VIDEO_LOG("0x78 LINE_NUM   *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_LINE_NUM.all,         IP_QSPI_SENSOR_IN->REG_LINE_NUM.all);
    VIDEO_LOG("0x7C CONFIG     *0x%08x=0x%08x", &IP_QSPI_SENSOR_IN->REG_CONFIG.all,           IP_QSPI_SENSOR_IN->REG_CONFIG.all);
}


void qspi_in_reset(void)
{
    ap_cfg_gpdma_clk_enable();
    ap_cfg_video_clk_enable();
    ap_cfg_vic_clk_enable();
    ap_cfg_dma_sel_qspi_in();
    ap_cfg_qspi0_clk_enable();
    ap_cfg_qspi0_clk_sel();
    ap_cfg_qspi0_clk_div_m(2);
    ap_cfg_qspi0_clk_div_n(1);
    ap_cfg_qspi0_clk_inv();
    ap_cfg_vic_reset();
    ap_cfg_qspi0_reset();
}



/********************** GPDMA *****************************************************************/
static volatile uint32_t qspi_in_gpdma_finish_cnt = 0;

uint32_t qspi_in_gpdma_get_cnt(void)
{
    return qspi_in_gpdma_finish_cnt;
}

static void qspi_in_gpdma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    qspi_in_gpdma_finish_cnt++;
}

int32_t qspi_in_gpdma_init(csk_gpdma_ch_t gpdma_ch)
{
    int32_t ret;

    csk_gpdma_init_t gpdma_cfg = {
            .dma_ch = gpdma_ch,
            .burst_len = gpdma_burst_len_8spl,
            .src_mode = address_mode_pipo,
            .dst_mode = address_mode_pipo,
            .tfr_mode = tfr_mode_p2m,
            .src_inc_mode = inc_mode_fix,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = dvp_hs_num5,
    };

    ret = GPDMA_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    DELAY_MS(10);

    ret = GPDMA_Config(&gpdma_cfg, qspi_in_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

int32_t qspi_in_gpdma_start(csk_gpdma_ch_t gpdma_ch, void *pbuf, uint32_t size_word)
{
    int32_t ret;

    CHECK_POINT_NOT_NULL(pbuf);

    ret = GPDMA_Start_PiPo(gpdma_ch, (uint32_t*)QSPI_SENSOR_IN0_Buf(), (uint32_t*)QSPI_SENSOR_IN0_Buf(), pbuf, pbuf, size_word);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

int32_t qspi_in_gpdma_stop(csk_gpdma_ch_t gpdma_ch)
{
    int32_t ret;

    ret = GPDMA_Stop(gpdma_ch);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

