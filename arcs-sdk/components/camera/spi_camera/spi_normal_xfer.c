#include "Driver_SPI.h"
#include "Driver_GPIO.h"
#include "ClockManager.h"
#include "IOMuxManager.h"
#include "dma.h"
// #include "log_print.h"
#include "lisa_log.h"
#include "camera_xfer.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#define SPI_XFER_NOCS       1

#define SPI_CAMERA_DATA_BITS        8
#define SPI_CAMERA_BIT_ORDER        CSK_SPI_LSB_MSB
#define SPI_CAMERA_MODE             CSK_SPI_CPOL0_CPHA1
#define SPI_CAMERA_ROLE             CSK_SPI_MODE_SLAVE
#define SPI_CAMERA_RX_IO            CSK_SPI_RXIO_AUTO

static void *spi_dev = NULL;
static void *cs_gpio_dev = NULL;
static uint8_t cs_pin = 0;
static uint8_t dma_channel = 0;
static struct cam_xfer_queue *spi_data_queue = NULL;
static void *stop_sem = NULL;
volatile static struct cam_ipeg_mem *g_spi_mem = NULL;
volatile static uint8_t stop_flag = 1;

_FAST_TEXT static void _spi_drv_event(uint32_t event, uint32_t usr_param)
{
    if (event != CSK_SPI_EVENT_TRANSFER_COMPLETE)
        LOGW("spi event: %x", (unsigned int)event);

    if (event & CSK_SPI_EVENT_TRANSFER_COMPLETE) {
#if !SPI_XFER_NOCS
        int ret = 0;
        struct cam_xfer_queue *queue = (struct cam_xfer_queue *)usr_param;
        BaseType_t yield = pdFALSE;

        if (stop_flag) {
            xQueueSendFromISR(queue->queue_in, &g_spi_mem, &yield);
            xSemaphoreGiveFromISR(stop_sem, &yield);

            return;
        }

        g_spi_mem->buf.readed = g_spi_mem->buf.size;
        ret = xQueueSendFromISR(queue->queue_out, &g_spi_mem, &yield);
        if (!ret) {
            LOGE("irq send q fail lost frame %d", x_queue_items(queue->queue_out));
            return;
        }

        if(!xQueueReceiveFromISR(queue->queue_in, &g_spi_mem, &yield)) {
            LOGE("irq get spi queue in error  %d", x_queue_items(queue->queue_in));
        }

        if (stop_flag) {
            xSemaphoreGiveFromISR(stop_sem);
            return ;
        }

        ret = SPI_Receive(spi_dev, g_spi_mem->buf.addr, g_spi_mem->buf.size);
        if (ret != CSK_DRIVER_OK) {
            LOGE("%s: SPI_Receive (spi_no = %d) call failed %d!!\r\n", __func__, SPI_Index(spi_dev), ret);
        }
#endif
    }
}

#if SPI_XFER_NOCS
_FAST_TEXT static void _spi_cs_set_cb(void *spi_dev, uint8_t level)
{
    // CLOGI("%s %d %d\r\n", __func__, SPI_Index(spi_dev), level);
    SPI_Pull_CS(spi_dev, level);
}

_FAST_TEXT static void _gpio_drv_event(uint32_t event, void *usr_param)
{
    struct cam_xfer_queue *queue = (struct cam_xfer_queue *)usr_param;
    BaseType_t yield = pdFALSE;

    // LOGI("spi camera cs pin interrupt: %x %x\n", event, GPIO_PinRead(cs_gpio_dev, (1 << cs_pin)));
    if (event & (1 << cs_pin)) {
        int ret = 0;
        if (stop_flag) {
            GPIO_Control(cs_gpio_dev, CSK_GPIO_INTR_DISABLE, (1UL << cs_pin));
            if (g_spi_mem)
                xQueueSendFromISR(queue->queue_in, &g_spi_mem, &yield);
            xSemaphoreGiveFromISR(stop_sem, &yield);

            return;
        }

        if (g_spi_mem)
            g_spi_mem->buf.readed = SPI_GetDataCount(spi_dev);
        SPI_Control(spi_dev, CSK_SPI_ABORT_TRANSFER, 0);
    
        if (g_spi_mem) {
            ret = xQueueSendFromISR(queue->queue_out, &g_spi_mem, &yield);
            if (!ret) {
                LOGE("irq send q fail lost frame %d", uxQueueMessagesWaitingFromISR(queue->queue_out));
                return;
            }
        }

        if(!xQueueReceiveFromISR(queue->queue_in, &g_spi_mem, &yield)) {
            LOGE("irq get spi queue in error  %d", uxQueueMessagesWaitingFromISR(queue->queue_in));
            g_spi_mem = NULL;
            return;
        }

        ret = SPI_Receive_NEnd(spi_dev, g_spi_mem->buf.addr, g_spi_mem->buf.size);
        if (ret != CSK_DRIVER_OK) {
            LOGE("%s: SPI_Receive (spi_no = %d) call failed %d!!\r\n", __func__, SPI_Index(spi_dev), ret);
        }
    }
}
#endif

static void _spi_xfer_pinmux(const spi_config_t *spi_config)
{
    IOMuxManager_PinConfigure(spi_config->pins.clk.pad, spi_config->pins.clk.pin, spi_config->pins.clk.func);
    IOMuxManager_PinConfigure(spi_config->pins.mosi.pad, spi_config->pins.mosi.pin, spi_config->pins.mosi.func);

#if SPI_XFER_NOCS
    IOMuxManager_PinConfigure(spi_config->pins.cs.pad, spi_config->pins.cs.pin, spi_config->pins.cs.func);
#else
    IOMuxManager_PinConfigure(spi_config->pins.cs.pad, spi_config->pins.cs.pin, spi_config->pins.cs.func);
#endif
}

int spi_normal_init(const xfer_hw_config_t *config, struct cam_xfer_queue *queue)
{
    int ret = 0;

    if (!queue) {
        LOGE("spi queue is null");
        return -1;
    }

    cs_gpio_dev = (config->spi_config.pins.cs.pad == CSK_IOMUX_PAD_A) ? GPIOA() : GPIOB();
    dma_channel = config->spi_config.dma_channel;
    cs_pin      = config->spi_config.pins.cs.pin;
    spi_dev     = config->spi_config.spi_dev;

    _spi_xfer_pinmux(&config->spi_config);

    stop_sem = xSemaphoreCreateBinary();

#if SPI_XFER_NOCS
    ret = SPI_Initialize_NCS(spi_dev, _spi_drv_event, (uint32_t)queue, _spi_cs_set_cb);
#else
    ret = SPI_Initialize(spi_dev, _spi_drv_event, (uint32_t)queue);
#endif
    if (ret != CSK_DRIVER_OK) {
        LOGE("%s: SPI_Initialize (spi_no = %d) call failed!!\r\n", __func__, SPI_Index(spi_dev));
        return ret;
    }

    spi_data_queue = queue;
    ret = SPI_PowerControl(spi_dev, CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK) {
        LOGE("%s: SPI_PowerControl (spi_no = %d) call failed!!\r\n", __func__, SPI_Index(spi_dev));
        return ret;
    }
    HAL_CRM_SetSpi0ClkSrc(CRM_IpSrcPeriClk);

    ret = SPI_Control(spi_dev, SPI_CAMERA_ROLE | SPI_CAMERA_RX_IO |
                                                                SPI_CAMERA_MODE | CSK_SPI_DATA_BITS(8) |
                                                                SPI_CAMERA_BIT_ORDER, 0);
    if (ret != CSK_DRIVER_OK) {
        LOGE("%s: SPI_Control (spi_no = %d) call failed!!\r\n", __func__, SPI_Index(spi_dev));
        return ret;
    }

    SPI_ADV_ATTR attr = {0};
    attr.flags = SPI_ATTR_RX_NSYNCA | SPI_ATTR_RX_DMACH_PRIO | SPI_ATTR_RX_DMACH_RSVD;

    attr.rx_nsynca     = 1;
    attr.rx_dmach_prio = 6;
    attr.rx_dmach_rsvd = dma_channel;

    SPI_Control(spi_dev, CSK_SPI_SET_ADV_ATTR, (uint32_t)&attr);
    dma_channel_unreserve(attr.rx_dmach_rsvd);

#if SPI_XFER_NOCS
    SPI_Enable_Pull_CS(spi_dev, true);

    GPIO_Initialize(cs_gpio_dev, NULL, NULL);
    GPIO_SetCallback(cs_gpio_dev, 1 << cs_pin, _gpio_drv_event, queue);
    GPIO_SetDir(cs_gpio_dev, 1 << cs_pin, CSK_GPIO_DIR_INPUT);
    GPIO_Control(cs_gpio_dev, CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_POSITIVE_EDGE |
                                CSK_GPIO_INTR_DISABLE, (1UL << cs_pin));
#endif

    LOGI("%s init success", __func__);
    return ret;
}

int spi_normal_deinit(void)
{
    if (spi_dev != NULL) {
        SPI_PowerControl(spi_dev, CSK_POWER_OFF);
        SPI_Uninitialize(spi_dev);
    }

    return 0;
}

int spi_normal_stop(void)
{
    if (stop_flag) {
        LOGW("camera spi already stop.");
        return 0;
    }
    stop_flag = 1;
    xSemaphoreTake(stop_sem, portMAX_DELAY);

    SPI_Control(spi_dev, CSK_SPI_ABORT_TRANSFER, 0);

    dma_channel_unreserve(dma_channel);
    return 0;
}

int spi_normal_resume(void)
{
    if (spi_dev != NULL) {
        SPI_Enable_Pull_CS(spi_dev, true);
    }

    return 0;
}

int spi_norm_start(void)
{
    SPI_ADV_ATTR attr = {0};
    int ret = 0;

    if (!stop_flag) {
        LOGW("camera spi already start.");
        return 0;
    }
    stop_flag = 0;

    LOGI("spi normal start.....%p %d", cs_gpio_dev, cs_pin);
    if(!xQueueReceive(spi_data_queue->queue_in, &g_spi_mem, pdMS_TO_TICKS(100))) {
        LOGE("get spi queue in error start");
        return -1;
    }

    attr.flags         = SPI_ATTR_RX_DMACH_RSVD;
    attr.rx_dmach_rsvd = dma_channel;
    SPI_Control(spi_dev, CSK_SPI_SET_ADV_ATTR, (uint32_t)&attr);

#if SPI_XFER_NOCS
    ECLIC_SetLevelIRQ(37, 15);
    ret = SPI_Receive_NEnd(spi_dev, g_spi_mem->buf.addr, g_spi_mem->buf.size);
    if (ret != CSK_DRIVER_OK) {
        LOGE("%s: SPI_Receive_NEnd (spi_no = %d) call failed %d!!\r\n", __func__, SPI_Index(spi_dev), ret);
    }
    GPIO_Control(cs_gpio_dev, CSK_GPIO_INTR_ENABLE, (1UL << cs_pin));
#else
    ret = SPI_Receive(spi_dev, g_spi_mem->buf.addr, g_spi_mem->buf.size);
#endif
    if (ret != CSK_DRIVER_OK) {
        LOGE("%s: SPI_Receive (spi_no = %d) call failed %d!!\r\n", __func__, SPI_Index(spi_dev), ret);
    }

    return 0;
}

static struct cam_xfer_ops spi_normal_xfer_ops = {
    .cam_xfer_init   = spi_normal_init,
    .cam_xfer_start  = spi_norm_start,
    .cam_xfer_stop   = spi_normal_stop,
    .cam_xfer_resume = spi_normal_resume,
    .cam_xfer_deinit = spi_normal_deinit,
    .cam_xfer_pp_recv = NULL,
};

static struct cam_xfer spi_normal_xfer = {
    .ops = &spi_normal_xfer_ops,
    .priv = NULL,
};

struct cam_xfer *spi_xfer_get(void)
{
    return &spi_normal_xfer;
}