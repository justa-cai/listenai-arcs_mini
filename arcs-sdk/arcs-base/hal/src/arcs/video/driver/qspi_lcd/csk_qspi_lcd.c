#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "chip.h"
#include "mmio.h"
#include "IOMuxManager.h"
#include "ClockManager.h"
#include "Driver_GPDMA.h"
#include "Driver_QSPI_LCD.h"
#include "log_print.h"
#include "csk_timer.h"
#include "csk_clk_reset.h"
#include "check.h"
#include "csk_qspi_lcd.h"


static void *gSpiDev = NULL;

/*SPI Event*/
//static volatile uint32_t SPIEvent = 0;
//SemaphoreHandle_t xSemaphore;
static volatile uint8_t g_run_flag = 0;

static inline void SET_XFER_DONE()
{
    g_run_flag = 1;
}

static inline void CLR_XFER_DONE()
{
    g_run_flag = 0;
}

static inline uint8_t GET_XFER_DONE()
{
    return g_run_flag;
}

// SPI event
static void QSPI_DrvEvent (uint32_t event, uint32_t usr_param)
{
    //notify SPI to send next block of data
    if (event & CSK_QSPI_LCD_EVENT_TRANSFER_COMPLETE)
    {
        SET_XFER_DONE();
    }

    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
}


int32_t qspi_lcd_init(qspi_lcd_config_t *qspi_cfg)
{
    int32_t ret = 0;
    uint32_t bus_speed = qspi_cfg->clk_hz;
    uint32_t control = 0;

    CHECK_POINT_NOT_NULL(qspi_cfg);

    gSpiDev = QSPI_LCD();
    CHECK_POINT_NOT_NULL(gSpiDev);

    //SPI configuration
    ret = QSPI_LCD_Initialize(gSpiDev, QSPI_DrvEvent, (uint32_t)gSpiDev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = QSPI_LCD_PowerControl(gSpiDev, CSK_POWER_FULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    switch (qspi_cfg->txio)
    {
        case QSPI_LCD_TXIO_PIO:
            control |= CSK_QSPI_LCD_TXIO_PIO;
            break;

        case QSPI_LCD_TXIO_DMA:
            control |= CSK_QSPI_LCD_TXIO_DMA;
            break;

        default:
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    switch (qspi_cfg->cp)
    {
        case QSPI_LCD_CPOL0_CPOH0:
            control |= CSK_QSPI_LCD_CPOL0_CPHA0;
            break;

        case QSPI_LCD_CPOL0_CPOH1:
            control |= CSK_QSPI_LCD_CPOL0_CPHA1;
            break;

        case QSPI_LCD_CPOL1_CPOH0:
            control |= CSK_QSPI_LCD_CPOL1_CPHA0;
            break;

        case QSPI_LCD_CPOL1_CPOH1:
            control |= CSK_QSPI_LCD_CPOL1_CPHA1;
            break;

        default:
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    if (true == qspi_cfg->is_msb) {
            control |= CSK_QSPI_LCD_MSB_LSB;
    } else {
            control |= CSK_QSPI_LCD_LSB_MSB;
    }

    ret = QSPI_LCD_Control(gSpiDev, control | CSK_QSPI_LCD_MODE_MASTER | CSK_QSPI_LCD_DATA_BITS(QSPI_LCD_DATA_BITS), bus_speed);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    bus_speed = QSPI_LCD_Control(gSpiDev, CSK_QSPI_LCD_GET_BUS_SPEED, bus_speed);
    //VIDEO_LOG("[%s:%d] Current actual SPI speed: %d", __func__, __LINE__, bus_speed);

    // divided by 2 if bus speed is greater than 100MHz
    if (bus_speed > 100000000) { // 100MHz
        bus_speed /= 2;
        QSPI_LCD_Control(gSpiDev, CSK_QSPI_LCD_SET_BUS_SPEED, bus_speed);
        bus_speed = QSPI_LCD_Control(gSpiDev, CSK_QSPI_LCD_GET_BUS_SPEED, 0);
        VIDEO_LOG("Current newest SPI speed: %d", bus_speed);
    }

    ret = QSPI_LCD_SetDatLength(gSpiDev, 8);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = QSPI_LCD_SetLcdMode(gSpiDev, CSK_QSPI_LCD_MODE_NORMAL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = qspi_lcd_set_lane_num(1);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    CLR_XFER_DONE();

    return ret;
}


int32_t qspi_lcd_deinit(void)
{
    int32_t ret = 0;

    gSpiDev = QSPI_LCD();
    CHECK_POINT_NOT_NULL(gSpiDev);

    //SPI configuration
    ret = QSPI_LCD_Uninitialize(gSpiDev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    CLR_XFER_DONE();

    return ret;
}


int32_t qspi_lcd_write(void *pdata, uint32_t num)
{
    int32_t ret;

    if((pdata == NULL) || (num == 0))
    {
        VIDEO_LOG("[%s:%d] invalid parameter", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    ret = QSPI_LCD_Control(gSpiDev, CSK_QSPI_LCD_RESET_FIFO, 0);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = QSPI_LCD_Send(gSpiDev, pdata, num);
    if(ret != CSK_DRIVER_OK) {
        VIDEO_LOG("pdata=0x%x, *pdata=0x%x, num=%d", pdata, *(uint8_t *)pdata, num);
    }
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    qspi_lcd_wait_done();

    return ret;
}


void qspi_lcd_wait_done(void)
{
    QSPI_LCD_Wait_Done(gSpiDev);
}


bool is_qspi_lcd_irq_tx_done(void)
{
    if (GET_XFER_DONE()) {
        CLR_XFER_DONE();
        return true;
    } else {
        return false;
    }
}


int32_t qspi_lcd_set_lane_num(uint8_t lane_num)
{
    int32_t ret = 0;

    switch (lane_num)
    {
        case 1:
            ret = QSPI_LCD_SetLaneNum(gSpiDev, CSK_QSPI_LCD_LANE_NUM_SINGLE);
            CHECK_RET_EQ(ret, CSK_DRIVER_OK);
            break;

        case 2:
            ret = QSPI_LCD_SetLaneNum(gSpiDev, CSK_QSPI_LCD_LANE_NUM_DUAL);
            CHECK_RET_EQ(ret, CSK_DRIVER_OK);
            break;

        case 4:
            ret = QSPI_LCD_SetLaneNum(gSpiDev, CSK_QSPI_LCD_LANE_NUM_QUAD);
            CHECK_RET_EQ(ret, CSK_DRIVER_OK);
            break;

        default:
            VIDEO_LOG("[%s:%d] (lane_num = %d) failed!!\r\n", __func__, __LINE__, lane_num);
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return ret;
}


int32_t qspi_lcd_set_data_bit(uint8_t bit)
{
    int32_t ret;

    ret = QSPI_LCD_Control(gSpiDev, CSK_QSPI_LCD_DATA_BITS(bit), 0);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t qspi_lcd_set_dma_size(uint32_t size_byte)
{
    int32_t ret;

    ret = QSPI_LCD_SetDMASize(gSpiDev, size_byte);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t qspi_lcd_dma_enable(void)
{
    int32_t ret;

    ret = QSPI_LCD_DMAEnable(gSpiDev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t qspi_lcd_dma_disable(void)
{
    int32_t ret;

    ret = QSPI_LCD_DMADisable(gSpiDev);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


void qspi_lcd_reset(void)
{
    ap_cfg_gpdma_clk_enable();
    ap_cfg_video_clk_enable();
    ap_cfg_qspi1_clk_enable();

    ap_cfg_qspi1_clk_sel();
    ap_cfg_qspi1_clk_div_m(2);
    ap_cfg_qspi1_clk_div_n(1);
    ap_cfg_qspi1_clk_inv();

    ap_cfg_dma_sel_qspi_out();
    ap_cfg_qspi1_reset();
}


void qspi_lcd_reg_dump(void)
{
    QSPI_LCD_RegDef *qspi_lcd = (QSPI_LCD_RegDef *)QSPI_LCD_BASE;

    VIDEO_LOG("0x00 REG_IDREV        *0x%08x=0x%08x", &qspi_lcd->REG_IDREV.all, qspi_lcd->REG_IDREV.all);
    VIDEO_LOG("0x10 REG_TRANSFMT     *0x%08x=0x%08x", &qspi_lcd->REG_TRANSFMT.all, qspi_lcd->REG_TRANSFMT.all);
    VIDEO_LOG("0x14 REG_DIRECTIO     *0x%08x=0x%08x", &qspi_lcd->REG_DIRECTIO.all, qspi_lcd->REG_DIRECTIO.all);
    VIDEO_LOG("0x20 REG_TRANSCTRL    *0x%08x=0x%08x", &qspi_lcd->REG_TRANSCTRL.all, qspi_lcd->REG_TRANSCTRL.all);
    VIDEO_LOG("0x24 REG_CMD          *0x%08x=0x%08x", &qspi_lcd->REG_CMD.all, qspi_lcd->REG_CMD.all);
    VIDEO_LOG("0x28 REG_ADDR         *0x%08x=0x%08x", &qspi_lcd->REG_ADDR.all, qspi_lcd->REG_ADDR.all);
    //VIDEO_LOG("0x2C REG_DATA         *0x%08x=0x%08x", &qspi_lcd->REG_DATA.all, qspi_lcd->REG_DATA.all);
    VIDEO_LOG("0x30 REG_CTRL         *0x%08x=0x%08x", &qspi_lcd->REG_CTRL.all, qspi_lcd->REG_CTRL.all);
    VIDEO_LOG("0x34 REG_STATUS       *0x%08x=0x%08x", &qspi_lcd->REG_STATUS.all, qspi_lcd->REG_STATUS.all);
    VIDEO_LOG("0x38 REG_INTREN       *0x%08x=0x%08x", &qspi_lcd->REG_INTREN.all, qspi_lcd->REG_INTREN.all);
    VIDEO_LOG("0x3C REG_INTRST       *0x%08x=0x%08x", &qspi_lcd->REG_INTRST.all, qspi_lcd->REG_INTRST.all);
    VIDEO_LOG("0x40 REG_TIMING       *0x%08x=0x%08x", &qspi_lcd->REG_TIMING.all, qspi_lcd->REG_TIMING.all);
    VIDEO_LOG("0x50 REG_MEMCTRL      *0x%08x=0x%08x", &qspi_lcd->REG_MEMCTRL.all, qspi_lcd->REG_MEMCTRL.all);
    VIDEO_LOG("0x60 REG_SLVST        *0x%08x=0x%08x", &qspi_lcd->REG_SLVST.all, qspi_lcd->REG_SLVST.all);
    VIDEO_LOG("0x64 REG_SLVDATACNT   *0x%08x=0x%08x", &qspi_lcd->REG_SLVDATACNT.all, qspi_lcd->REG_SLVDATACNT.all);
    VIDEO_LOG("0x68 REG_LCD_TX       *0x%08x=0x%08x", &qspi_lcd->REG_LCD_TX.all, qspi_lcd->REG_LCD_TX.all);
    VIDEO_LOG("0x7C REG_CONFIG       *0x%08x=0x%08x", &qspi_lcd->REG_CONFIG.all, qspi_lcd->REG_CONFIG.all);
}



/****************************************** GPDMA ************************************************/
static volatile uint32_t qspi_lcd_gpdma_finish_cnt = 0;

static void qspi_lcd_gpdma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    qspi_lcd_gpdma_finish_cnt++;
}


int32_t qspi_lcd_gpdma_init(csk_gpdma_ch_t gpdma_ch)
{
    int32_t ret;

    csk_gpdma_init_t gpdma_cfg = {
            .dma_ch = gpdma_ch,
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

    qspi_lcd_gpdma_finish_cnt = 0;

    ret = GPDMA_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&gpdma_cfg, qspi_lcd_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return CSK_DRIVER_OK;
}


int32_t qspi_lcd_gpdma_start(csk_gpdma_ch_t gpdma_ch, void* pbuf, uint32_t size_word)
{
    int32_t ret;

    CHECK_POINT_NOT_NULL(pbuf);

    qspi_lcd_gpdma_finish_cnt = 0;

    ret = GPDMA_Start_Normal(gpdma_ch, pbuf, (void*)QSPI_LCD_Buf(), size_word);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


int32_t qspi_lcd_gpdma_stop(csk_gpdma_ch_t gpdma_ch)
{
    int32_t ret;

    ret = GPDMA_Stop(gpdma_ch);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


uint32_t qspi_lcd_gpdma_get_cnt(void)
{
    return qspi_lcd_gpdma_finish_cnt;
}




