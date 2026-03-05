#include <stdint.h>
#include <string.h>

#include "lisa_device.h"
#include "lisa_spi.h"
#include <lisa_semaphore.h>

#include "Driver_SPI.h"
#include "Driver_GPIO.h"
#include "lisa_gpio.h"
#include "lisa_camera_bus.h"
#include "dma.h"

#define TAG "camera_bus_spi"
#include "lisa_log.h"

/* SPI 驱动上下文结构体 */
typedef struct {
    void *spi_dev;                                       /* SPI 设备句柄 */
    lisa_device_t *cs_gpio_dev;                          /* CS GPIO lisa 设备 */
    uint8_t cs_pin;                                      /* CS 引脚 */
    uint8_t dma_channel;                                 /* DMA 通道 */
    lisa_semaphore_t *stop_sem;                           /* 停止信号量 */
    lisa_camera_frame_callback_t callback;               /* 帧完成回调 */
    lisa_camera_get_free_fb_t get_free_fb;               /* 获取空闲帧回调 */
    lisa_camera_get_free_fb_from_isr_t get_free_fb_isr;  /* 获取空闲帧回调(ISR) */
    void *user_data;                                     /* 用户数据 */
    lisa_camera_fb_t *current_fb;                        /* 当前帧缓冲区 */
    volatile uint8_t stop_flag;                          /* 停止标志 */
} caemra_bus_spi_priv_t;

static caemra_bus_spi_priv_t camera_bus_spi_priv = {
    .spi_dev = NULL,
    .cs_gpio_dev = NULL,
    .cs_pin = 0,
    .dma_channel = 0,
    .stop_sem = NULL,
    .callback = NULL,
    .get_free_fb = NULL,
    .get_free_fb_isr = NULL,
    .user_data = NULL,
    .current_fb = NULL,
    .stop_flag = 1,
};

static void _spi_drv_event(uint32_t event, uint32_t usr_param)
{
    caemra_bus_spi_priv_t *priv = (caemra_bus_spi_priv_t*)usr_param;
    lisa_camera_fb_t *completed_fb = NULL;
    lisa_camera_fb_t *next_fb = NULL;

    if (event != CSK_SPI_EVENT_TRANSFER_COMPLETE) {
        LOGW("spi event: %x", (unsigned int)event);
    }

    if ((event & CSK_SPI_EVENT_TRANSFER_COMPLETE) && !priv->cs_gpio_dev) {
        /* 有 CS 模式: SPI 传输完成即表示一帧结束 */
        if (priv->stop_flag) {
            lisa_semaphore_give(priv->stop_sem);
            return;
        }

        completed_fb = priv->current_fb;

        /* 通过回调获取下一个空闲帧缓冲区并继续接收 */
        if (priv->get_free_fb_isr) {
            next_fb = priv->get_free_fb_isr(priv->user_data);
            if (next_fb) {
                /* 有空闲缓冲区,使用新缓冲区继续接收 */
                priv->current_fb = next_fb;
                SPI_Receive(priv->spi_dev, priv->current_fb->buf, priv->current_fb->len);
            } else {
                /* 没有空闲缓冲区,复用当前缓冲区继续接收,丢弃本帧数据 */
                LOGW("No free frame buffer available, reuse current buffer and drop frame");
                SPI_Receive(priv->spi_dev, priv->current_fb->buf, priv->current_fb->len);
                completed_fb = NULL;  /* 不通知上层,丢弃本帧 */
            }
        }

        /* 通知上层帧已完成 (仅当成功获取到新缓冲区时才交付数据) */
        if (priv->callback && completed_fb) {
            // completed_fb->len = SPI_GetDataCount(priv->spi_dev);
            priv->callback(completed_fb, priv->user_data);
        }
    }
}

/* NOCS 模式: CS 引脚控制回调 */
_FAST_TEXT static void _spi_cs_set_cb(void *spi_dev, uint8_t level)
{
    SPI_Pull_CS(spi_dev, level);
}

/* NOCS 模式: GPIO 中断处理函数，检测 CS 上升沿表示帧结束 */
_FAST_TEXT static void _gpio_drv_event(uint32_t pin, void *user_data)
{
    caemra_bus_spi_priv_t *priv = (caemra_bus_spi_priv_t*)user_data;
    lisa_camera_fb_t *completed_fb = NULL;
    lisa_camera_fb_t *next_fb = NULL;

    if (priv->stop_flag) {
        lisa_gpio_disable_irq(priv->cs_gpio_dev, priv->cs_pin);
        lisa_semaphore_give(priv->stop_sem);
        return;
    }

    /* 获取实际接收的数据长度 */
    if (priv->current_fb) {
        // priv->current_fb->len = SPI_GetDataCount(priv->spi_dev);
    }
    SPI_Control(priv->spi_dev, CSK_SPI_ABORT_TRANSFER, 0);

    completed_fb = priv->current_fb;

    /* 通过回调获取下一个空闲帧缓冲区 */
    if (priv->get_free_fb_isr) {
        int ret;
        next_fb = priv->get_free_fb_isr(priv->user_data);
        if (next_fb) {
            /* 有空闲缓冲区,使用新缓冲区继续接收 */
            priv->current_fb = next_fb;
            ret = SPI_Receive_NEnd(priv->spi_dev, priv->current_fb->buf, priv->current_fb->len);
            if (ret != CSK_DRIVER_OK) {
                LOGE("SPI_Receive_NEnd failed %d", ret);
            }
        } else {
            /* 没有空闲缓冲区,复用当前缓冲区继续接收,丢弃本帧数据 */
            LOGW("No free frame buffer available, reuse current buffer and drop frame");
            ret = SPI_Receive_NEnd(priv->spi_dev, priv->current_fb->buf, priv->current_fb->len);
            if (ret != CSK_DRIVER_OK) {
                LOGE("SPI_Receive_NEnd failed %d", ret);
            }
            completed_fb = NULL;  /* 不通知上层,丢弃本帧 */
        }
    }

    /* 通知上层帧已完成 (仅当成功获取到新缓冲区时才交付数据) */
    if (priv->callback && completed_fb) {
        priv->callback(completed_fb, priv->user_data);
    }
}

static int lisa_camera_bus_spi_init(lisa_device_t *dev, const lisa_camera_bus_config_t *bus_config)
{
    int ret = 0;
    caemra_bus_spi_priv_t *priv = (caemra_bus_spi_priv_t*)dev->priv_data;
    const lisa_camera_bus_spi_config_t *spi_config = &bus_config->config.spi;

    if (bus_config->bus_type != LISA_CAMERA_BUS_SPI) {
        LOGE("Invalid bus type %d", bus_config->bus_type);
        return LISA_DEVICE_ERR_INVALID;
    }

    if (strcmp(spi_config->spi_dev->name, "spi0") == 0) {
        priv->spi_dev = SPI0();
    }
    else if (strcmp(spi_config->spi_dev->name, "spi1") == 0) {
        priv->spi_dev = SPI1();
    }
    else if (strcmp(spi_config->spi_dev->name, "spi2") == 0) {
        priv->spi_dev = SPI2();
    }


    priv->dma_channel = bus_config->dma_channel;

    priv->stop_sem = lisa_semaphore_create(1);

    SPI_Uninitialize(priv->spi_dev);

    if (bus_config->config.spi.cs_gpio) {
        ret = SPI_Initialize_NCS(priv->spi_dev, _spi_drv_event, priv, _spi_cs_set_cb);
    }
    else {
        ret = SPI_Initialize(priv->spi_dev, _spi_drv_event, priv);
    }
    if (ret != CSK_DRIVER_OK) {
        LOGE("SPI_Initialize failed %d", ret);
        return ret;
    }

    ret = SPI_PowerControl(priv->spi_dev, CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK) {
        LOGE("SPI_PowerControl failed %d", ret);
        return ret;
    }

    uint32_t control = CSK_SPI_MODE_SLAVE | CSK_SPI_DATA_BITS(8);
    if (spi_config->spi_bit_order == 0) {
        control |= CSK_SPI_MSB_LSB;  /* MSB first */
    }
    else {
        control |= CSK_SPI_LSB_MSB;  /* LSB first */
    }

    switch (spi_config->spi_mode) {
    case 0:
        control |= CSK_SPI_CPOL0_CPHA0;
        break;
    case 1:
        control |= CSK_SPI_CPOL0_CPHA1;
        break;
    case 2:
        control |= CSK_SPI_CPOL1_CPHA0;
        break;
    case 3:
        control |= CSK_SPI_CPOL1_CPHA1;
        break;
    default:
        LOGE("Invalid SPI mode %d", spi_config->spi_mode);
        return LISA_DEVICE_ERR_INVALID;
    }

    ret = SPI_Control(priv->spi_dev, control, spi_config->spi_freq);
    if (ret != CSK_DRIVER_OK) {
        LOGE("SPI_Control failed %d", ret);
        return ret;
    }

    SPI_ADV_ATTR attr = {0};
    attr.flags = SPI_ATTR_RX_NSYNCA | SPI_ATTR_RX_DMACH_PRIO | SPI_ATTR_RX_DMACH_RSVD;

    attr.rx_nsynca     = 1;
    attr.rx_dmach_prio = 6;
    attr.rx_dmach_rsvd = priv->dma_channel;

    ret = SPI_Control(priv->spi_dev, CSK_SPI_SET_ADV_ATTR, (uint32_t)&attr);
    if (ret != CSK_DRIVER_OK) {
        LOGE("SPI_Control ADV_ATTR failed %d", ret);
        return ret;
    }
    dma_channel_unreserve(attr.rx_dmach_rsvd);

    if (spi_config->cs_gpio) {
        SPI_Enable_Pull_CS(priv->spi_dev, spi_config->cs_pin);

        priv->cs_gpio_dev = spi_config->cs_gpio;
        priv->cs_pin = spi_config->cs_pin;
        lisa_gpio_configure(spi_config->cs_gpio, spi_config->cs_pin, LISA_GPIO_CONFIG_INPUT_PULLUP);
        lisa_gpio_configure_irq(spi_config->cs_gpio, spi_config->cs_pin, LISA_GPIO_IRQ_EDGE_RISING,
                                        _gpio_drv_event, priv);
    }

    LOGI("SPI camera bus init success (NOCS=%d)", spi_config->cs_pin);
    return ret;
}

static int lisa_camera_bus_spi_start_capture(lisa_device_t *dev, lisa_camera_frame_callback_t callback,
                                              lisa_camera_get_free_fb_t get_free_fb,
                                              lisa_camera_get_free_fb_from_isr_t get_free_fb_from_isr,
                                              void *data)
{
    int ret = 0;
    caemra_bus_spi_priv_t *priv = (caemra_bus_spi_priv_t*)dev->priv_data;
    if (!priv->stop_flag) {
        LOGW("camera spi already start.");
        return 0;
    }

    /* 保存回调函数 */
    priv->callback = callback;
    priv->get_free_fb = get_free_fb;
    priv->get_free_fb_isr = get_free_fb_from_isr;
    priv->user_data = data;

    /* 通过回调获取第一个空闲帧缓冲区 */
    priv->current_fb = get_free_fb ? get_free_fb(data) : NULL;
    if (!priv->current_fb) {
        LOGE("No free frame buffer available");
        return -1;
    }

    priv->stop_flag = 0;
    SPI_ADV_ATTR attr = {
        .flags = SPI_ATTR_RX_DMACH_RSVD,
        .rx_dmach_rsvd = priv->dma_channel,
    };
    SPI_Control(priv->spi_dev, CSK_SPI_SET_ADV_ATTR, (uint32_t)&attr);

    if (priv->cs_gpio_dev) {
        /* NOCS 模式: 使用 SPI_Receive_NEnd 并启用 GPIO 中断 */
        ret = SPI_Receive_NEnd(priv->spi_dev, priv->current_fb->buf, priv->current_fb->len);
        if (ret != CSK_DRIVER_OK) {
            LOGE("SPI_Receive_NEnd failed %d", ret);
            priv->stop_flag = 1;
            return ret;
        }
        if (priv->cs_gpio_dev) {
            lisa_gpio_enable_irq(priv->cs_gpio_dev, priv->cs_pin);
        }
    }
    else {
        /* 有 CS 模式: 使用 SPI_Receive */
        ret = SPI_Receive(priv->spi_dev, priv->current_fb->buf, priv->current_fb->len);
        if (ret != CSK_DRIVER_OK) {
            LOGE("SPI_Receive failed %d", ret);
            priv->stop_flag = 1;
        }
    }
    return ret;
}

static int lisa_camera_bus_spi_stop_capture(lisa_device_t *dev)
{
    caemra_bus_spi_priv_t *priv = (caemra_bus_spi_priv_t*)dev->priv_data;
    if (priv->stop_flag) {
        LOGW("camera spi already stop.");
        return 0;
    }
    priv->stop_flag = 1;

    if (priv->cs_gpio_dev) {
        /* NOCS 模式: 等待 GPIO 中断处理完成 */
        lisa_semaphore_take(priv->stop_sem, LISA_WAIT_FOREVER);
    }

    SPI_Control(priv->spi_dev, CSK_SPI_ABORT_TRANSFER, 0);

    if (priv->current_fb != NULL) {
        lisa_camera_release_fb(dev, priv->current_fb);
        priv->current_fb = NULL;
    }

    dma_channel_unreserve(priv->dma_channel);

    return 0;
}

const lisa_camera_bus_if_t lisa_camera_bus_spi_if = {
    .init = lisa_camera_bus_spi_init,
    .start_capture = lisa_camera_bus_spi_start_capture,
    .stop_capture = lisa_camera_bus_spi_stop_capture,
};

static int camera_bus_spi_init(void)
{
    return 0;
}
LISA_DEVICE_REGISTER(camera_bus, &lisa_camera_bus_spi_if, &camera_bus_spi_priv, NULL, camera_bus_spi_init, LISA_DEVICE_PRIORITY_HIGH);
