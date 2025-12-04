#include <stdint.h>
#include <string.h>

#include "Driver_Common.h"
#include "Driver_GPDMA.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "Driver_QSPI_LCD.h"

#include "display_trans_ctx.h"
#include "lisa_log.h"
#include "lisa_display.h"

#define DRV_QSPI_CLK_HZ_DEFAULT (50*1000*1000) // 50MHz

static SemaphoreHandle_t g_qspi_dma_sem = NULL;
static void *cs_dev = NULL;
static uint8_t cs_pin = 0;
static uint8_t qspi_dma_channel = 0;

static void qspi_lcd_gpdma_callback(uint32_t event, void *workspace)
{
    if (!g_qspi_dma_sem) {
        return ;
    }

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(g_qspi_dma_sem, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

int qspi_trans_init(void *conf)
{
    struct qspi_config *config = conf;
    int ret = 0;

    g_qspi_dma_sem = xSemaphoreCreateBinary();
    if (!g_qspi_dma_sem) {
        LOGE("[%s] Failed to create DMA semaphore", __func__);
        return -1;
    }

    // for (int i = 0; i < 4; i++) {
    //     IOMuxManager_PinConfigure(config->qspi_pins.dio[i].pad, config->qspi_pins.dio[i].pin, config->qspi_pins.dio[i].func);
    // }

    // IOMuxManager_PinConfigure(config->qspi_pins.clk.pad, config->qspi_pins.clk.pin, config->qspi_pins.clk.func);
    // IOMuxManager_PinConfigure(config->qspi_pins.cs.pad, config->qspi_pins.cs.pin, config->qspi_pins.cs.func);

    cs_dev = config->qspi_pins.cs.pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
    cs_pin = config->qspi_pins.cs.pin;
    GPIO_Initialize(cs_dev, NULL, NULL);
    GPIO_SetDir(cs_dev, (1UL << cs_pin), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(cs_dev, (1UL << cs_pin), 1);

    qspi_dma_channel = config->qspi_tx_dma_ch;

    csk_gpdma_init_t gpdma_cfg = {
        .dma_ch       = qspi_dma_channel,
        .src_mode     = address_mode_normal,
        .dst_mode     = address_mode_normal,
        .tfr_mode     = tfr_mode_m2p,
        .prio_lvl     = prio_mode_vhigh,
        .handshake    = qspi_hs_num0,
        .burst_len    = gpdma_burst_len_2spl,
        .sample_unit  = gpdma_sample_unit_halfword,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
    };

    ret = GPDMA_Initialize();
    if (ret != CSK_DRIVER_OK) {
        LOGE("[%s] Failed to initialize GPDMA", __func__);
        return -1;
    }

    ret = GPDMA_Config(&gpdma_cfg, qspi_lcd_gpdma_callback, NULL);
    if (ret != CSK_DRIVER_OK) {
        LOGE("[%s] Failed to config GPDMA", __func__);
        return -1;
    }

    ret = QSPI_LCD_Initialize(QSPI_LCD(), NULL, (uint32_t)QSPI_LCD());
    if (ret != CSK_DRIVER_OK) {
        LOGE("[%s] Failed to initialize QSPI device", __func__);
        return -1;
    }

    ret = QSPI_LCD_PowerControl(QSPI_LCD(), CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK) {
        LOGE("[%s] Failed to power up QSPI device", __func__);
        return -1;
    }

    uint32_t control = CSK_QSPI_LCD_TXIO_PIO | CSK_QSPI_LCD_CPOL0_CPHA0 | 
                        CSK_QSPI_LCD_MSB_LSB | CSK_QSPI_LCD_MODE_MASTER |
                        CSK_QSPI_LCD_DATA_BITS(8);
    ret = QSPI_LCD_Control(QSPI_LCD(), control, (config->qspi_sck_freq<=0)?DRV_QSPI_CLK_HZ_DEFAULT:config->qspi_sck_freq);
    if (ret != CSK_DRIVER_OK) {
        LOGE("[%s] Failed to control QSPI device", __func__);
        return -1;
    }

    ret = QSPI_LCD_SetDatLength(QSPI_LCD(), 8);
    if (ret != CSK_DRIVER_OK) {
        LOGE("[%s] Failed to set data length", __func__);
        return -1;
    }

    ret = QSPI_LCD_SetLcdMode(QSPI_LCD(), CSK_QSPI_LCD_MODE_NORMAL);
    if (ret != CSK_DRIVER_OK) {
        CLOGE("[%s] Failed to set LCD mode", __FUNCTION__);
        return -1;
    }

    ret = QSPI_LCD_SetLaneNum(QSPI_LCD(), CSK_QSPI_LCD_LANE_NUM_SINGLE);
    if (ret != CSK_DRIVER_OK) {
        CLOGE("[%s] Failed to set lane number", __FUNCTION__);
        return -1;
    }

    return 0;
}

int qspi_trans_cmd(uint8_t *data, uint32_t data_len)
{
    QSPI_LCD_Control(QSPI_LCD(), CSK_QSPI_LCD_RESET_FIFO, 0);
    QSPI_LCD_Send(QSPI_LCD(), data, data_len);
    QSPI_LCD_Wait_Done(QSPI_LCD());

    return 0;
}

int qspi_trans_image(void *buf, uint32_t size)
{
    QSPI_LCD_DMADisable(QSPI_LCD());

    QSPI_LCD_SetDatLength(QSPI_LCD(), 16);
    QSPI_LCD_SetLaneNum(QSPI_LCD(), CSK_QSPI_LCD_LANE_NUM_QUAD);
    QSPI_LCD_SetDMASize(QSPI_LCD(), size);

    QSPI_LCD_DMAEnable(QSPI_LCD());
    GPDMA_Start_Normal(qspi_dma_channel, buf, (void *)QSPI_LCD_Buf(), size/2);

    return 0;
}

int qspi_trans_image_wait(uint32_t timeout_ms)
{
    if (xSemaphoreTake(g_qspi_dma_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        LOGE("[%s] Failed to take qspi dma semaphore", __func__);
        return -1;
    }

    QSPI_LCD_DMADisable(QSPI_LCD());

    QSPI_LCD_SetLaneNum(QSPI_LCD(), CSK_QSPI_LCD_LANE_NUM_SINGLE);
	QSPI_LCD_SetDatLength(QSPI_LCD(), 8);

    return 0;
}

void qspi_trans_cs_trig(uint8_t level)
{
    if (level)
        GPIO_PinWrite(cs_dev, (1UL << cs_pin), 1);
    else
        GPIO_PinWrite(cs_dev, (1UL << cs_pin), 0);
}

struct display_trans_ops qspi_trans_ops = {
    .trans_cmd        = qspi_trans_cmd,
    .trans_init       = qspi_trans_init,
    .trans_image      = qspi_trans_image,
    .trans_image_wait = qspi_trans_image_wait,
    .trans_cs_trig    = qspi_trans_cs_trig,
};  

struct display_trans_ops *display_trans_ops_get(void)
{
    return &qspi_trans_ops;
}