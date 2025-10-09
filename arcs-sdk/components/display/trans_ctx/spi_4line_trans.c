#include "Driver_Common.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "Driver_SPI.h"
#include "ClockManager.h"
#include "dma.h"

#include "lisa_display.h"
#include "display_trans_ctx.h"
#include "lisa_log.h"

#define _PER_SPI_BUS_SPEED_DEFAULT (50*1000*1000) // 50MHz

void *spi_dev;
uint8_t cs_pad, cs_pin;
uint8_t dc_pad, dc_pin;

static SemaphoreHandle_t send_done_sem;

static void _display_spi_drvevent(uint32_t event, uint32_t usr_param)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // LOGD("[%s] event:%d", __FUNCTION__, event);

    if (event & CSK_SPI_EVENT_TRANSFER_COMPLETE) {
        xSemaphoreGiveFromISR(send_done_sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

int spi_4line_trans_init(void *conf)
{
    struct spi_4line_config *config = conf;
    send_done_sem = xSemaphoreCreateBinary();
    if (send_done_sem == NULL) {
        // LOGE("[%s] Failed to create done semaphore", __FUNCTION__);
        return -1;
    }

    spi_dev = config->spi_dev;
    cs_pad = config->spi_pins.cs.pad;
    cs_pin = config->spi_pins.cs.pin;
    dc_pad = config->spi_pins.dc.pad;
    dc_pin = config->spi_pins.dc.pin;

    void *gpio_dev = config->spi_pins.cs.pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
    GPIO_Initialize(gpio_dev, NULL, NULL);
    IOMuxManager_PinConfigure(config->spi_pins.cs.pad, 
        config->spi_pins.cs.pin, config->spi_pins.cs.func);
    GPIO_SetDir(gpio_dev, (1UL << config->spi_pins.cs.pin), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gpio_dev, (1UL << config->spi_pins.cs.pin), 1);

    // cmd/data
    gpio_dev = config->spi_pins.dc.pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
    IOMuxManager_PinConfigure(config->spi_pins.dc.pad, 
        config->spi_pins.dc.pin, config->spi_pins.dc.func);
    GPIO_SetDir(gpio_dev, (1UL << config->spi_pins.dc.pin), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gpio_dev, (1UL << config->spi_pins.dc.pin), 1);

    IOMuxManager_PinConfigure(config->spi_pins.clk.pad, 
        config->spi_pins.clk.pin, config->spi_pins.clk.func);
    IOMuxManager_PinConfigure(config->spi_pins.sda.pad, 
        config->spi_pins.sda.pin, config->spi_pins.sda.func);
    HAL_CRM_SetSpi1ClkSrc(CRM_IpSrcPeriClk);
    SPI_Initialize(config->spi_dev, _display_spi_drvevent, 0);
    SPI_PowerControl(config->spi_dev, CSK_POWER_FULL);
    SPI_Control(config->spi_dev,
                CSK_SPI_MODE_MASTER | CSK_SPI_TXIO_PIO | CSK_SPI_CPOL1_CPHA1 | CSK_SPI_DATA_BITS(8) | CSK_SPI_MSB_LSB,
                (config->spi_sck_freq<=0)?_PER_SPI_BUS_SPEED_DEFAULT:config->spi_sck_freq);
    SPI_Control(config->spi_dev, CSK_SPI_GET_BUS_SPEED, 0);

    SPI_ADV_ATTR spi_attr = {0};
    spi_attr.flags = SPI_ATTR_TX_DMACH_RSVD | SPI_ATTR_TX_NSYNCA | SPI_ATTR_TX_DMA_BSIZE;
    spi_attr.tx_dmach_rsvd = config->spi_tx_dma_ch;
    spi_attr.tx_nsynca = 1;
    spi_attr.tx_dma_bsize = DMA_WIDTH_HALFWORD;
    SPI_Control(config->spi_dev, CSK_SPI_SET_ADV_ATTR, (uint32_t)&spi_attr);

    return 0;
}

int spi_4line_trans_cmd(uint8_t *data, uint32_t data_len)
{
    if (SPI_Send(spi_dev, (const void *)data, data_len) != CSK_DRIVER_OK) {
        // LOGE("[%s] Failed to send data", __FUNCTION__);
        return -1;
    }

    if (xSemaphoreTake(send_done_sem, pdMS_TO_TICKS(100)) != pdTRUE) {
        // LOGE("[%s] Failed to take send done semaphore", __FUNCTION__);
    }

    return 0;
}

int spi_4line_trans_image(void *buf, uint32_t size)
{
    SPI_Control(spi_dev, CSK_SPI_TXIO_DMA | CSK_SPI_DATA_BITS(16), 0);

    if (CSK_DRIVER_OK != SPI_Send(spi_dev, buf, size/2)) {
        // LOGE("[%s] Failed to send data", __FUNCTION__);
        return -1;
    }


    return 0;
}

int spi_4line_trans_image_wait(uint32_t timeout_ms)
{
    if (xSemaphoreTake(send_done_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        // LOGE("[%s] Failed to take send done semaphore", __FUNCTION__);
    }

    SPI_Control(spi_dev, CSK_SPI_TXIO_PIO | CSK_SPI_DATA_BITS(8), 0);
    return 0;
}

void spi_4line_trans_cs_trig(uint8_t level)
{
    void *gpio_dev = (cs_pad == CSK_IOMUX_PAD_A) ? GPIOA() : GPIOB();
    if (level)
        GPIO_PinWrite(gpio_dev, (1UL << cs_pin), 1);
    else
        GPIO_PinWrite(gpio_dev, (1UL << cs_pin), 0);
}

void spi_4line_trans_dc_trig(uint8_t level)
{
    void *gpio_dev = cs_pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
    if (level)
        GPIO_PinWrite(gpio_dev, (1UL << dc_pin), 1);
    else
        GPIO_PinWrite(gpio_dev, (1UL << dc_pin), 0);
}

struct display_trans_ops spi_4line_trans_ops = {
    .trans_cmd        = spi_4line_trans_cmd,
    .trans_init       = spi_4line_trans_init,
    .trans_image      = spi_4line_trans_image,
    .trans_image_wait = spi_4line_trans_image_wait,
    .trans_cs_trig    = spi_4line_trans_cs_trig,
    .trans_dc_trig    = spi_4line_trans_dc_trig,
};

struct display_trans_ops *display_trans_ops_get(void)
{
    return &spi_4line_trans_ops;
}