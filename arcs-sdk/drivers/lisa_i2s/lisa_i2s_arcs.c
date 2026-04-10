/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "Driver_I2S.h"
#include "cache.h"
#include "lisa_device.h"
#include "lisa_i2s.h"

#include "FreeRTOS.h"
#include "projdefs.h"
#include "task.h"
#include "semphr.h"


#define LOG_TAG "lisa_i2s_arcs"
#include <lisa_log.h>

#define I2S_CLK_IN_24M 24000000

typedef enum {
    LISA_I2S_STATE_IDLE,
    LISA_I2S_STATE_HARDWARE_INIT,
    LISA_I2S_STATE_SOFTWARE_INIT,
    LISA_I2S_STATE_CONFIGURED,
} lisa_i2s_state_e;

typedef enum {
    LISA_I2S_STREAM_STATE_IDLE,
    LISA_I2S_STREAM_STATE_INIT,
    LISA_I2S_STREAM_STATE_ENABLE,
} lisa_i2s_stream_state_e;

typedef struct {
    uint8_t *data;
    uint32_t len;
} lisa_i2s_msg_t;

typedef struct {
    lisa_i2s_stream_state_e state;
	QueueHandle_t queue;
    uint8_t *fifo[CONFIG_LISA_I2S_BLOCK_COUNT];
    volatile uint32_t index;
    uint32_t fifo_size;
} lisa_i2s_stream_t;

/* I2S 私有数据结构 */
typedef struct {
    void *hal_dev;                          /* HAL 层 I2S 设备句柄 */
    uint32_t id;                            /* I2S ID */

    lisa_i2s_config_t config;               /* 当前配置 */
    lisa_i2s_event_callback_t callback;     /* 事件回调 */
    void *user_data;                        /* 用户数据 */

    uint32_t one_slot_size;                 /* 每个采样点的大小 */
    uint32_t slot_cnt;                      /* 一个sample对应的通道数 */
    uint32_t one_transfer_size;             /* 一次传输的大小 */
    uint32_t one_transfer_recv_cnt;         /* 一个传输对应的接收通道总数 */

    lisa_i2s_state_e state;
    lisa_i2s_stream_t tx_stream;
    lisa_i2s_stream_t rx_stream;

    SemaphoreHandle_t mutex;                /* 互斥锁 */
} lisa_i2s_priv_t;

/* I2S0 和 I2S1 静态私有数据 */
#ifdef CONFIG_LISA_I2S0
static lisa_i2s_priv_t i2s0_priv = {0};
#endif

#ifdef CONFIG_LISA_I2S1
static lisa_i2s_priv_t i2s1_priv = {0};
#endif

#define ALIGN32(x) (((x) + 31) / 32 * 32)

static int arcs_i2s_stop(lisa_device_t *dev, lisa_i2s_direction_t dir);
/* ===== API 实现函数 ===== */

void i2s_drv_event_callback(uint32_t event_info, uint32_t usr_param)
{
    int ret;
    bool start_now = true;
    lisa_i2s_msg_t tx_msg;
    uint8_t *buffer;
    uint32_t yield = pdFALSE;
    uint32_t recv_cnt = 0;

    lisa_device_t *device = (lisa_device_t *)usr_param;
    lisa_i2s_priv_t *i2s_priv = (lisa_i2s_priv_t *)device->priv_data;
    void *i2s_dev = i2s_priv->hal_dev;
    lisa_i2s_stream_t *rx_stream = &i2s_priv->rx_stream;
    lisa_i2s_stream_t *tx_stream = &i2s_priv->tx_stream;
    lisa_i2s_slot_mask_t slot_mask = i2s_priv->config.slot_mask;

    uint16_t event = event_info & CSK_I2S_EVENT_MASK; //use 13bits event type?

    LISA_LOGD(LOG_TAG, "event: %d", event);

    if(event & (CSK_I2S_EVENT_RECEIVE_COMPLETE
#if CONFIG_LISA_I2S_PIPO
        | CSK_I2S_EVENT_RX_BLOCK_COMPLETE
#endif
    )) {
        recv_cnt = I2S_GetRxCount(i2s_dev, slot_mask);
        if (recv_cnt != i2s_priv->one_transfer_recv_cnt){
            LISA_LOGE(LOG_TAG, "recved_cnt: %d != should_recv_cnt:%d", recv_cnt, i2s_priv->one_transfer_recv_cnt);
        }

        buffer = rx_stream->fifo[rx_stream->index];
        dcache_invalidate_range(buffer, buffer + rx_stream->fifo_size);

        ret = xQueueSendFromISR(rx_stream->queue, &buffer, &yield);
        if (ret != pdPASS) {
            LISA_LOGE(LOG_TAG, "rx buffer full");
            // arcs_i2s_stop(device, LISA_I2S_DIRECTION_RX);
        }

        rx_stream->index = (rx_stream->index + 1) % CONFIG_LISA_I2S_BLOCK_COUNT;

    #if CONFIG_LISA_I2S_PIPO
        uint8_t rxcnt = 1;
        ret = I2S_Receive_PiPo(i2s_dev, (PIPO_IN_BLOCK[]){
                { .sample_data = (void *)(rx_stream->fifo[rx_stream->index]), .sample_cnt = i2s_priv->one_transfer_recv_cnt, .flags = 0 },
            }, &rxcnt, slot_mask, start_now);
    #else
        ret = I2S_Receive(i2s_dev, (uint32_t *)rx_stream->fifo[rx_stream->index], i2s_priv->one_transfer_recv_cnt, slot_mask, start_now);
    #endif

        if (i2s_priv->callback) {
            i2s_priv->callback(LISA_I2S_EVENT_RX_DONE, i2s_priv->user_data);
        }
    }

    if (event & (CSK_I2S_EVENT_RX_FIFO_OVERRUN | CSK_I2S_EVENT_RX_FIFO_FULL)) {
        arcs_i2s_stop(device, LISA_I2S_DIRECTION_RX);
        if (i2s_priv->callback) {
            i2s_priv->callback(LISA_I2S_EVENT_RX_OVERRUN, i2s_priv->user_data);
        }
    }

    if (event & (CSK_I2S_EVENT_TRANSMIT_COMPLETE
    #if CONFIG_LISA_I2S_PIPO
        | CSK_I2S_EVENT_TX_BLOCK_COMPLETE
    #endif
    )) {
        /* 发送完成，然后queue_receive这个已经发送的缓存， 方便i2s_write继续send */
        ret = xQueueReceiveFromISR(tx_stream->queue, &tx_msg,  &yield);
        if (ret != pdPASS) {
            LISA_LOGE(LOG_TAG, "%s, %d", __FUNCTION__, __LINE__);
        }

        /* 看一下还有没有数据要发送，如果有先复制出来，然后I2S_Send，发送完成后再QueueReceive */
        ret = xQueuePeekFromISR(tx_stream->queue, &tx_msg);
        if (ret != pdPASS) {
            LISA_LOGE(LOG_TAG, "%s, %d, no buffer to send", __FUNCTION__, __LINE__);
        } else {
    #if CONFIG_LISA_I2S_PIPO
            uint8_t txcnt = 1;
            ret = I2S_Send_PiPo(i2s_dev, (PIPO_OUT_BLOCK[]){
                    { .sample_data = (void *)(tx_msg.data), .sample_cnt = tx_msg.len, .flags = 0 },
                }, &txcnt, slot_mask, start_now);
    #else
            ret = I2S_Send(i2s_dev, (uint32_t *)tx_msg.data, tx_msg.len, slot_mask, start_now);
    #endif
        }

        if (i2s_priv->callback) {
            i2s_priv->callback(LISA_I2S_EVENT_TX_DONE, i2s_priv->user_data);
        }
    }

    if (event & (CSK_I2S_EVENT_TX_FIFO_UNDERRUN | CSK_I2S_EVENT_TX_FIFO_EMPTY)) {
        arcs_i2s_stop(device, LISA_I2S_DIRECTION_TX);
    
        if (i2s_priv->callback) {
            i2s_priv->callback(LISA_I2S_EVENT_TX_UNDERRUN, i2s_priv->user_data);
        }
    }

#if CONFIG_LISA_I2S_ECHO
    if (event & (CSK_I2S_EVENT_ECHO_RX_COMPLETE
    #if CONFIG_LISA_I2S_PIPO
        | CSK_I2S_EVENT_ECHO_RX_BLOCK_COMPLETE
    #endif
    )) {

    }

    if (event & (   CSK_I2S_EVENT_ECHO_RX_FIFO_OVERRUN)) {
        
    }

#endif

    if (event & (CSK_I2S_EVENT_CLOCK_ERROR | CSK_I2S_EVENT_OTHER_ERROR)) {
        arcs_i2s_stop(device, i2s_priv->config.direction);
        LISA_LOGE(LOG_TAG, "I2S error 0x%x", event);

        if (i2s_priv->callback) {
            i2s_priv->callback(LISA_I2S_EVENT_ERROR, i2s_priv->user_data);
        }
    }

    portYIELD_FROM_ISR(yield);
}

static int calculate_one_slot_size(const lisa_i2s_config_t *config)
{
    int width = 0;

    if (config->data_width <= LISA_I2S_DATA_WIDTH_16BIT) {
        width = 2;
    } else {
        width = 4;
    }

    return width;
}

static int calculate_one_sample_slot_count(const lisa_i2s_config_t *config)
{
    if (config->use_tdm) {
        return config->tdm_slots;
    } else {
        if (config->slot_mask == LISA_I2S_SLOT_STEREO) {
            return 2;
        } else {
            return 1;
        }
    }
}

static int arcs_i2s_software_init(lisa_device_t *dev, const lisa_i2s_config_t *config)
{
    if (!dev || !dev->priv_data || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = 0;
    uint32_t dir = config->direction;
    lisa_i2s_priv_t *priv = (lisa_i2s_priv_t *)dev->priv_data;

    LISA_LOGD(LOG_TAG, "I2S%d software init", ((lisa_i2s_priv_t *)dev->priv_data)->id);

    /* 计算缓存大小 */
    priv->one_slot_size = calculate_one_slot_size(config);
    priv->slot_cnt = calculate_one_sample_slot_count(config);

    /* TX和RX的fifo_size配置 */
    priv->one_transfer_size = config->block_size;
    priv->tx_stream.fifo_size = priv->one_transfer_size;
    priv->rx_stream.fifo_size = priv->one_transfer_size;
    
    /* RX每次接收的通道总个数 */
    priv->one_transfer_recv_cnt = config->block_size / priv->one_slot_size;

    LISA_LOGI(LOG_TAG, "one transfer size: %d", priv->one_transfer_size);

    /* 创建互斥锁 */
    priv->mutex = xSemaphoreCreateMutex();
    if (!priv->mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex for I2S0");
        goto I2S_INIT_FAILED;
    }
    
    if (dir & LISA_I2S_DIRECTION_TX) {
        priv->tx_stream.queue = xQueueCreate(CONFIG_LISA_I2S_BLOCK_COUNT, sizeof(lisa_i2s_msg_t));
        if (!priv->tx_stream.queue) {
            LISA_LOGE(LOG_TAG, "Failed to create tx queue for I2S%d", priv->id);
            goto I2S_INIT_FAILED;
        }

        for (int i = 0; i < CONFIG_LISA_I2S_BLOCK_COUNT; i++) {
            priv->tx_stream.fifo[i] = psram_calloc_align(32, priv->one_transfer_size, 1);
            if (!priv->tx_stream.fifo[i]) {
                LISA_LOGE(LOG_TAG, "Failed to allocate TX FIFO %d", i);
                goto I2S_INIT_FAILED;
            }

            LISA_LOGD(LOG_TAG, "TX FIFO[i]: %p allocated", priv->tx_stream.fifo[i]);
        }

        xQueueReset(priv->tx_stream.queue);
        priv->tx_stream.index = 0;
        priv->tx_stream.state = LISA_I2S_STREAM_STATE_INIT;
    }

    if (dir & LISA_I2S_DIRECTION_RX) {
        priv->rx_stream.queue = xQueueCreate(CONFIG_LISA_I2S_BLOCK_COUNT, sizeof(uint8_t *));
        if (!priv->rx_stream.queue) {
            LISA_LOGE(LOG_TAG, "Failed to create rx queue for I2S%d", priv->id); 
            goto I2S_INIT_FAILED;
        }

        for (int i = 0; i < CONFIG_LISA_I2S_BLOCK_COUNT; i++) {
            priv->rx_stream.fifo[i] = psram_calloc_align(32, priv->one_transfer_size, 1);
            if (!priv->rx_stream.fifo[i]) {
                LISA_LOGE(LOG_TAG, "Failed to allocate RX FIFO %d", i);
                goto I2S_INIT_FAILED;
            }

            LISA_LOGD(LOG_TAG, "RX FIFO[i]: %p allocated", priv->rx_stream.fifo[i]);
        }

        xQueueReset(priv->rx_stream.queue);
        priv->rx_stream.index = 0;
        priv->rx_stream.state = LISA_I2S_STREAM_STATE_INIT;
    }

    priv->state = LISA_I2S_STATE_SOFTWARE_INIT;

    LISA_LOGD(LOG_TAG, "I2S%d software init success", priv->id);

    return LISA_DEVICE_OK;

I2S_INIT_FAILED:
    if (priv->mutex) vSemaphoreDelete(priv->mutex);

    if (dir & LISA_I2S_DIRECTION_TX) {
        if (priv->tx_stream.queue) vQueueDelete(priv->tx_stream.queue);
        priv->tx_stream.queue = NULL;

        for (int i = 0; i < CONFIG_LISA_I2S_BLOCK_COUNT; i++) {
            if (priv->tx_stream.fifo[i]) {
                psram_free(priv->tx_stream.fifo[i]);
            }
        }

        priv->tx_stream.state = LISA_I2S_STREAM_STATE_IDLE;
    }

    if (dir & LISA_I2S_DIRECTION_RX) {
        if (priv->rx_stream.queue) vQueueDelete(priv->rx_stream.queue);
        priv->rx_stream.queue = NULL;

        for (int i = 0; i < CONFIG_LISA_I2S_BLOCK_COUNT; i++) {
            if (priv->rx_stream.fifo[i]) {
                psram_free(priv->rx_stream.fifo[i]);
            }
        }
        
        priv->rx_stream.state = LISA_I2S_STREAM_STATE_IDLE;
    }

    return LISA_DEVICE_ERR_INIT_FAIL;
}

static int arcs_i2s_hardware_init(lisa_device_t *dev, const lisa_i2s_config_t *config)
{
    if (!dev || !dev->priv_data || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    LISA_LOGD(LOG_TAG, "I2S%d hardware init", ((lisa_i2s_priv_t *)dev->priv_data)->id);

    int ret = 0;
    uint32_t dev_bmp_flag = 0;
    uint32_t dir = config->direction;
    uint32_t slot_mask = config->slot_mask;
    lisa_i2s_priv_t *priv = (lisa_i2s_priv_t *)dev->priv_data;

    if (((slot_mask & LISA_I2S_SLOT_STEREO) == 0) || ((dir & LISA_I2S_DIRECTION_BOTH) == 0)) {
        LISA_LOGE(LOG_TAG, "Invalid slot mask=0x%x or direction=0x%x", slot_mask, dir);
        return LISA_DEVICE_ERR_INVALID;
    }

    /* dma channel和声道配置 */
    I2S_DMA_CHS dmach;
    memset(&dmach, 0xFF, sizeof(dmach));

    if (dir & LISA_I2S_DIRECTION_RX) {
        if (slot_mask & LISA_I2S_SLOT_LEFT) { // left and stereo
            dmach.dma_ch_in_left = priv->id ? CONFIG_LISA_I2S1_RX_DMA_CHANNEL : CONFIG_LISA_I2S0_RX_DMA_CHANNEL;
        } else { // only right
            dmach.dma_ch_in_right = priv->id ? CONFIG_LISA_I2S1_RX_DMA_CHANNEL : CONFIG_LISA_I2S0_RX_DMA_CHANNEL;
        }

        dev_bmp_flag |= slot_mask << I2S_BMP_FLAG_IN_POS;
    }

    if (dir & LISA_I2S_DIRECTION_TX) {
        if (slot_mask & LISA_I2S_SLOT_LEFT) { // left and stereo
            dmach.dma_ch_out_left = priv->id ? CONFIG_LISA_I2S1_TX_DMA_CHANNEL : CONFIG_LISA_I2S0_TX_DMA_CHANNEL;
        } else { // only right
            dmach.dma_ch_out_right = priv->id ? CONFIG_LISA_I2S1_TX_DMA_CHANNEL : CONFIG_LISA_I2S0_TX_DMA_CHANNEL;
        }

        dev_bmp_flag |= slot_mask << I2S_BMP_FLAG_OUT_POS;
    }

    if (config->echo.enable) {
        // TODO
        LISA_LOGE(LOG_TAG, "I2S echo is not supported");
        return LISA_DEVICE_ERR_INVALID;

        if (config->echo.slot_mask & LISA_I2S_SLOT_LEFT) { // left and stereo
            dmach.dma_ch_echo_left = CONFIG_LISA_I2S_ECHO_DMA_CHANNEL;
        } else { // only right
            dmach.dma_ch_echo_right = CONFIG_LISA_I2S_ECHO_DMA_CHANNEL;
        }

        dev_bmp_flag |= config->echo.slot_mask << I2S_BMP_FLAG_ECHO_POS;
    }

    LISA_LOGD(LOG_TAG, "I2S%d hardware init, dev_bmp_flag=0x%x, dmach=0x%x", priv->id, dev_bmp_flag, dmach);

    ret = I2S_Initialize(priv->hal_dev, 
                    i2s_drv_event_callback, 
                    dev, 
                    dev_bmp_flag,
                    &dmach);
    if ((ret != CSK_DRIVER_OK) && (ret != CSK_I2S_ERROR_INITED_ALREADY)) {
        LISA_LOGE(LOG_TAG, "Failed to initialize I2S%d", priv->id);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    ret = I2S_PowerControl(priv->hal_dev, CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "Failed to set I2S%d power control", priv->id);
        I2S_Uninitialize(priv->hal_dev);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    LISA_LOGD(LOG_TAG, "I2S%d hardware init success", priv->id);

    priv->state = LISA_I2S_STATE_HARDWARE_INIT;

    return LISA_DEVICE_OK;
}

static int arcs_i2s_configure(lisa_device_t *dev, const lisa_i2s_config_t *config)
{
    if (!dev || !dev->priv_data || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    int ret = LISA_DEVICE_OK;
    uint32_t control = 0;
    uint32_t protocol = 0;
    uint32_t format = 0;
    uint32_t sample_rate = 0;
    uint32_t bclk_lrclk_ratio = 0;
    uint32_t argv = 0;

    bool is_master = false;
    lisa_i2s_priv_t *priv = (lisa_i2s_priv_t *)dev->priv_data;

    /* 如果已经配置过，需要先停止并清理资源，以支持重新配置 */
    if (priv->state >= LISA_I2S_STATE_CONFIGURED) {
        LISA_LOGI(LOG_TAG, "I2S%d already configured, stopping and cleaning up for reconfiguration", priv->id);
        
        /* 停止所有传输 */
        if (priv->tx_stream.state == LISA_I2S_STREAM_STATE_ENABLE) {
            arcs_i2s_stop(dev, LISA_I2S_DIRECTION_TX);
        }
        
        if (priv->rx_stream.state == LISA_I2S_STREAM_STATE_ENABLE) {
            arcs_i2s_stop(dev, LISA_I2S_DIRECTION_RX);
        }
        
        /* 清理硬件资源 */
        if (priv->state >= LISA_I2S_STATE_HARDWARE_INIT) {
            /* 关闭电源 */
            ret = I2S_PowerControl(priv->hal_dev, CSK_POWER_OFF);
            if (ret != CSK_DRIVER_OK) {
                LISA_LOGW(LOG_TAG, "I2S%d PowerControl(OFF) failed: %d", priv->id, ret);
            }
            
            /* 去初始化硬件 */
            ret = I2S_Uninitialize(priv->hal_dev);
            if (ret != CSK_DRIVER_OK) {
                LISA_LOGW(LOG_TAG, "I2S%d Uninitialize failed: %d", priv->id, ret);
            }
            
            LISA_LOGD(LOG_TAG, "I2S%d hardware deinitialized", priv->id);
        }
        
        /* 清理软件资源 */
        if (priv->mutex) {
            vSemaphoreDelete(priv->mutex);
            priv->mutex = NULL;
        }
        
        /* 清理 TX 资源 */
        if (priv->tx_stream.queue) {
            vQueueDelete(priv->tx_stream.queue);
            priv->tx_stream.queue = NULL;
        }
        for (int i = 0; i < CONFIG_LISA_I2S_BLOCK_COUNT; i++) {
            if (priv->tx_stream.fifo[i]) {
                psram_free(priv->tx_stream.fifo[i]);
                priv->tx_stream.fifo[i] = NULL;
            }
        }
        priv->tx_stream.state = LISA_I2S_STREAM_STATE_IDLE;
        
        /* 清理 RX 资源 */
        if (priv->rx_stream.queue) {
            vQueueDelete(priv->rx_stream.queue);
            priv->rx_stream.queue = NULL;
        }
        for (int i = 0; i < CONFIG_LISA_I2S_BLOCK_COUNT; i++) {
            if (priv->rx_stream.fifo[i]) {
                psram_free(priv->rx_stream.fifo[i]);
                priv->rx_stream.fifo[i] = NULL;
            }
        }
        priv->rx_stream.state = LISA_I2S_STREAM_STATE_IDLE;
        
        /* 重置状态 */
        priv->state = LISA_I2S_STATE_IDLE;
        
        LISA_LOGI(LOG_TAG, "I2S%d cleanup completed (hardware + software), ready for reconfiguration", priv->id);
    }

    /* block_size */
    if (config->block_size == 0) {
        LISA_LOGE(LOG_TAG, "block_size cannot be 0");
        return LISA_DEVICE_ERR_INVALID;
    } else {
        if (config->block_size % 32 != 0) {
            LISA_LOGE(LOG_TAG, "block_size must be multiple of 32");
            return LISA_DEVICE_ERR_INVALID;
        }
    }

    /* 主从模式 */
    is_master = (config->mode == LISA_I2S_MODE_MASTER);
    if (is_master) {
        control |= CSK_I2S_MODE_MASTER;
        LISA_LOGD(LOG_TAG, "I2S is in master mode");
    } else {
        control |= CSK_I2S_MODE_SLAVE;
        LISA_LOGD(LOG_TAG, "I2S is in slave mode");
    }

    /* 数据宽度 */
    switch (config->data_width) {
        case LISA_I2S_DATA_WIDTH_16BIT:
            format = CSK_I2S_DATA_FORMAT_DUAL_16BIT;
            break;
        case LISA_I2S_DATA_WIDTH_20BIT_HIGH:
            format = CSK_I2S_DATA_FORMAT_20BIT_HIGH;
            break;
        case LISA_I2S_DATA_WIDTH_24BIT_HIGH:
            format = CSK_I2S_DATA_FORMAT_24BIT_HIGH;
            break;
        case LISA_I2S_DATA_WIDTH_24BIT_LOW:
            format = CSK_I2S_DATA_FORMAT_24BIT_LOW;
            break;
        case LISA_I2S_DATA_WIDTH_32BIT:
            format = CSK_I2S_DATA_FORMAT_32BIT;
            break;
        default:
            LISA_LOGE(LOG_TAG, "Invalid I2S data width: %d", config->data_width);
            return LISA_DEVICE_ERR_INVALID;
    }
    control |= format;
    
    /* 位序 */
    if (config->bit_order == LISA_I2S_BIT_ORDER_MSB) {
        control |= CSK_I2S_BIT_ORDER_MSB;
    } else {
        control |= CSK_I2S_BIT_ORDER_LSB;
    }

    /* 协议 */
    switch (config->protocol) {
        case LISA_I2S_PROTOCOL_PHILIPS:
            protocol = CSK_I2S_PROTO_PHILIPS;
            break;
        case LISA_I2S_PROTOCOL_LEFT_JUSTIFIED:
            protocol = CSK_I2S_PROTO_LEFT;
            break;
        case LISA_I2S_PROTOCOL_RIGHT_JUSTIFIED:
            protocol = CSK_I2S_PROTO_RIGHT;
            break;
        case LISA_I2S_PROTOCOL_PCM_SHORT_MODE_0:
            protocol = CSK_I2S_PROTO_PCMMODE_0;
            break;
        case LISA_I2S_PROTOCOL_PCM_SHORT_MODE_1:
            protocol = CSK_I2S_PROTO_PCMMODE_1;
            break;
        default:
            LISA_LOGE(LOG_TAG, "Invalid I2S protocol: %d", config->protocol);
            return LISA_DEVICE_ERR_INVALID;
    }
    control |= protocol;

    /* RX */
    if (config->direction & LISA_I2S_DIRECTION_RX) {
        if (config->slot_mask == LISA_I2S_SLOT_STEREO) {
            control |= CSK_I2S_RXCH_MIXED;
        } else {
            control |= CSK_I2S_RXCH_SEPA;
        }
    }

    /* TX */
    if (config->direction & LISA_I2S_DIRECTION_TX) {
        if (config->slot_mask == LISA_I2S_SLOT_STEREO) {
            control |= CSK_I2S_TXCH_STEREO_SRC_STEREO;
        } else {
            control |= CSK_I2S_TXCH_MONO_SRC_MONO;
        }
    }

    if (config->echo.enable) {
        // TODO
        LISA_LOGE(LOG_TAG, "I2S echo is not supported");
        return LISA_DEVICE_ERR_INVALID;
    }


    /* tdm slots */
    if (config->use_tdm) {
        if (config->tdm_slots >= 2 && config->tdm_slots <= 16) {
            control |= CSK_I2S_TDM_CHS(config->tdm_slots);
        } else {
            control |= CSK_I2S_TDM_CHS(0);
            LISA_LOGE(LOG_TAG, "I2S tdm slots=%d, is invalid", config->tdm_slots);
            return LISA_DEVICE_ERR_INVALID;
        }
    } else {
        control |= CSK_I2S_TDM_CHS(0);
    }

    /* 采样率 */
    switch (config->sample_rate) {
        case LISA_I2S_SAMPLE_RATE_8K:
        case LISA_I2S_SAMPLE_RATE_16K:
        case LISA_I2S_SAMPLE_RATE_24K:
        case LISA_I2S_SAMPLE_RATE_32K:
        case LISA_I2S_SAMPLE_RATE_48K:
        case LISA_I2S_SAMPLE_RATE_96K:
            sample_rate = config->sample_rate;
            break;
        default:
            LISA_LOGE(LOG_TAG, "Invalid I2S sample rate:%d", config->sample_rate);
            return LISA_DEVICE_ERR_INVALID;
    }

    if (is_master) { 
        argv = sample_rate;
    } else {
        if (config->protocol == LISA_I2S_PROTOCOL_RIGHT_JUSTIFIED) {
            /* BCLK/LRCLK ratio */
            bclk_lrclk_ratio = I2S_CLK_IN_24M / sample_rate;
            argv = bclk_lrclk_ratio;

            LISA_LOGI(LOG_TAG, "I2S slave mode with %dHz sample rate, BCLK/LRCLK ratio: %d", sample_rate, bclk_lrclk_ratio);
        } else {
            argv = 0;
        }

    }

    /* I2S真正初始化 */
    ret = arcs_i2s_hardware_init(dev, config);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = arcs_i2s_software_init(dev, config);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    LISA_LOGI(LOG_TAG, "I2S%d Control: control=0x%x, argv=0x%x", priv->id, control, argv);

    /* I2S参数配置 */
    // MODE(1:0) + PROTOCOL(4:2) + FORMAT(7:5) + TDM(11:8) + TXCFG(16:14) + ORDER(18:17)
    ret = I2S_Control(priv->hal_dev, control, argv);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "I2S_Control failed: %d", ret);
        return LISA_DEVICE_ERR_IO;
    }
    
    /* 保存配置 */
    memcpy(&priv->config, config, sizeof(lisa_i2s_config_t));

    /* 标记为已配置 */
    priv->state = LISA_I2S_STATE_CONFIGURED;
    
    LISA_LOGI(LOG_TAG, "I2S configured: mode=%d, protocol=%d, data_width=%d, bit_order: %d, sample_rate=%d, slot_mask=%d, direction=%d, echo=%d, echo_slot_mask=%d, use_tdm=%d, tdm_slots=%d, block_size:%d",
                        config->mode, 
                        config->protocol, 
                        config->data_width, 
                        config->bit_order,
                        config->sample_rate, 
                        config->slot_mask,
                        config->direction,
                        config->echo.enable,
                        config->echo.slot_mask,
                        config->use_tdm,
                        config->tdm_slots,
                        config->block_size);
    
    return LISA_DEVICE_OK;
}

static int arcs_i2s_get_config(lisa_device_t *dev, lisa_i2s_config_t *config)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_i2s_priv_t *priv = (lisa_i2s_priv_t *)dev->priv_data;
    
    if (priv->state < LISA_I2S_STATE_CONFIGURED) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    memcpy(config, &priv->config, sizeof(lisa_i2s_config_t));
    return LISA_DEVICE_OK;
}

static int arcs_i2s_set_callback(lisa_device_t *dev, lisa_i2s_event_callback_t callback, void *user_data)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_i2s_priv_t *priv = (lisa_i2s_priv_t *)dev->priv_data;

    if (callback) {
        priv->callback = callback;
    }

    priv->user_data = user_data;
    
    return LISA_DEVICE_OK;
}

static int arcs_i2s_write(lisa_device_t *dev, uint32_t *data, uint32_t cnt, uint32_t timeout_ms)
{
    if (!dev || !dev->priv_data || !data || cnt == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    int ret = 0;
    bool is_timeout = true;

    lisa_i2s_msg_t msg;
    lisa_i2s_priv_t *priv = (lisa_i2s_priv_t *)dev->priv_data;
    lisa_i2s_stream_t *tx_stream = &priv->tx_stream;
    
    if (priv->state < LISA_I2S_STATE_CONFIGURED || tx_stream->state < LISA_I2S_STREAM_STATE_INIT) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    is_timeout = true;
    
    /* 等待队列有空间，支持timeout_ms=0的情况（非阻塞） */
    do {
        if (uxQueueSpacesAvailable(tx_stream->queue)) {
            is_timeout = false;
            break;
        }
        
        if (timeout_ms == 0) {
            break;  /* 非阻塞模式，立即返回 */
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
        timeout_ms--;
    } while (timeout_ms > 0);

    if (!is_timeout) {
        memcpy(tx_stream->fifo[tx_stream->index], (uint8_t *)data, cnt * sizeof(uint32_t));
        dcache_flush_range((uint32_t)tx_stream->fifo[tx_stream->index], (uint32_t)tx_stream->fifo[tx_stream->index] + tx_stream->fifo_size);
    
        msg.data = tx_stream->fifo[tx_stream->index];

        if (priv->one_slot_size == 2) {
            msg.len = cnt * 2;
        } else {
            msg.len = cnt;
        }

        ret = xQueueSend(tx_stream->queue, &msg, 0);
        if (ret != pdTRUE) {
            LISA_LOGE(LOG_TAG, "I2S write failed ret:%d", ret);
        } else {
            tx_stream->index = (tx_stream->index + 1) % CONFIG_LISA_I2S_BLOCK_COUNT;
            LISA_LOGD(LOG_TAG, "send ok");
            ret = LISA_DEVICE_OK;
        }
    } else {
        ret = LISA_DEVICE_ERR_TIMEOUT;
    }
    
    return ret;
}

static int arcs_i2s_read(lisa_device_t *dev, uint8_t **data, uint32_t *len, uint32_t timeout_ms)
{
    if (!dev || !dev->priv_data || !data || !len) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    uint8_t *rx_data = NULL;
    lisa_i2s_priv_t *priv = (lisa_i2s_priv_t *)dev->priv_data;
    lisa_i2s_stream_t *rx_stream = &priv->rx_stream;
    
    if (priv->state < LISA_I2S_STATE_CONFIGURED || rx_stream->state < LISA_I2S_STREAM_STATE_ENABLE) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (xQueueReceive(rx_stream->queue, &rx_data, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return LISA_DEVICE_ERR_TIMEOUT;
    }

    *data = (uint8_t *)rx_data;
    *len = rx_stream->fifo_size;

    LISA_LOGD(LOG_TAG, "I2S read: len=%d, len=%d", rx_stream->fifo_size, *len);
    
    return LISA_DEVICE_OK;
}

static int arcs_i2s_start(lisa_device_t *dev, lisa_i2s_direction_t dir)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    int ret = 0;
    int cb_bmp_tx = 0;
    int cb_bmp_rx = 0;
    lisa_i2s_msg_t tx_msg = {0};
    bool start_now = false;

    lisa_i2s_priv_t *priv = (lisa_i2s_priv_t *)dev->priv_data;
    lisa_i2s_slot_mask_t slot_mask = priv->config.slot_mask;

    if ((dir & LISA_I2S_DIRECTION_RX) && (priv->rx_stream.state < LISA_I2S_STREAM_STATE_INIT)) {
        return LISA_DEVICE_ERR_BUSY;
    }

    if ((dir & LISA_I2S_DIRECTION_TX) && (priv->tx_stream.state < LISA_I2S_STREAM_STATE_INIT)) {
        return LISA_DEVICE_ERR_BUSY;
    }
    
    if (dir & LISA_I2S_DIRECTION_RX) {

        cb_bmp_rx = slot_mask;

        priv->rx_stream.index = 0;
        
        /* 清空接收数据队列（queue用于存放已接收的数据，初始应为空） */
        xQueueReset(priv->rx_stream.queue);

        LISA_LOGI(LOG_TAG, "I2S start RX: data=%p, len=%d, slot_mask=%d, start_now=%d", 
                                        priv->rx_stream.fifo[0], 
                                        priv->one_transfer_recv_cnt, slot_mask, start_now);

        /* 启动第一次接收到fifo[0] */
    #if CONFIG_LISA_I2S_PIPO
        uint8_t rxcnt = 1;
        ret = I2S_Receive_PiPo(priv->hal_dev, (PIPO_IN_BLOCK[]){
                { .sample_data = (void *)(priv->rx_stream.fifo[0]), .sample_cnt = priv->one_transfer_recv_cnt, .flags = 0 },
            }, &rxcnt, slot_mask, start_now);

    #else
        ret = I2S_Receive(priv->hal_dev, (uint32_t *)priv->rx_stream.fifo[0], priv->one_transfer_recv_cnt, slot_mask, start_now);
    #endif
    }

    if (dir & LISA_I2S_DIRECTION_TX) {

        cb_bmp_tx = slot_mask;

        ret = xQueuePeek(priv->tx_stream.queue, &tx_msg, 0);
        if (ret != pdTRUE) {
            LISA_LOGE(LOG_TAG, "I2S start: failed to receive tx message");
            return LISA_DEVICE_ERR_INVALID;
        }

        LISA_LOGI(LOG_TAG, "I2S start TX: data=%p, len=%d, slot_mask=%d, start_now=%d", tx_msg.data, tx_msg.len, slot_mask, start_now);

    #if CONFIG_LISA_I2S_PIPO
        uint8_t txcnt = 1;
        ret = I2S_Send_PiPo(priv->hal_dev, (PIPO_OUT_BLOCK[]){
                { .sample_data = (void *)(tx_msg.data), .sample_cnt = tx_msg.len, .flags = 0 },
            }, &txcnt, slot_mask, start_now);
    #else
        ret = I2S_Send(priv->hal_dev, (uint32_t *)tx_msg.data, tx_msg.len, slot_mask, start_now);
    #endif
    }

    LISA_LOGD(LOG_TAG, "I2S cb_bmp_rx: 0x%x, cb_bmp_tx: 0x%x", cb_bmp_rx, cb_bmp_tx);

    if (dir & LISA_I2S_DIRECTION_BOTH) {
        ret = I2S_Enable_Channels(priv->hal_dev, cb_bmp_rx, cb_bmp_tx);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "I2S enable channels failed: %d", ret);
            return ret;
        }

        if (cb_bmp_rx) {
            priv->rx_stream.state = LISA_I2S_STREAM_STATE_ENABLE;
        }
        if (cb_bmp_tx) {
            priv->tx_stream.state = LISA_I2S_STREAM_STATE_ENABLE;
        }

        LISA_LOGI(LOG_TAG, "I2S %s %s started", 
                        cb_bmp_rx ? "RX" : "",
                        cb_bmp_tx ? "TX" : "");
    }

    return LISA_DEVICE_OK;
}

static int arcs_i2s_drop(lisa_device_t *dev, lisa_i2s_direction_t dir)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    int ret = 0;
    int cb_bmp_tx = 0;
    int cb_bmp_rx = 0;
    lisa_i2s_priv_t *priv = (lisa_i2s_priv_t *)dev->priv_data;
    lisa_i2s_slot_mask_t slot_mask = priv->config.slot_mask;

    if ((dir & LISA_I2S_DIRECTION_RX) && (priv->rx_stream.state < LISA_I2S_STREAM_STATE_ENABLE)) {
        return LISA_DEVICE_OK;
    }

    if ((dir & LISA_I2S_DIRECTION_TX) && (priv->tx_stream.state < LISA_I2S_STREAM_STATE_ENABLE)) {
        return LISA_DEVICE_OK;
    }

    if (dir & LISA_I2S_DIRECTION_TX) {
        cb_bmp_tx = slot_mask;
    }

    if (dir & LISA_I2S_DIRECTION_RX) {
        cb_bmp_rx = slot_mask;
    }

    if (dir & LISA_I2S_DIRECTION_BOTH) {
        ret = I2S_Disable_Channels(priv->hal_dev, cb_bmp_rx, cb_bmp_tx);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "I2S disable channels failed: %d", ret);
            return ret;
        }
        
        if (cb_bmp_tx) {
            priv->tx_stream.index = 0;
            xQueueReset(priv->tx_stream.queue);
            priv->tx_stream.state = LISA_I2S_STREAM_STATE_INIT;
        }

        if (cb_bmp_rx) {
            priv->rx_stream.index = 0;
            xQueueReset(priv->rx_stream.queue);
            priv->rx_stream.state = LISA_I2S_STREAM_STATE_INIT;
        }

        LISA_LOGI(LOG_TAG, "I2S %s %s disabled", 
                        cb_bmp_rx ? "RX" : "",
                        cb_bmp_tx ? "TX" : "");
    }
    
    return LISA_DEVICE_OK;
}

static int arcs_i2s_stop(lisa_device_t *dev, lisa_i2s_direction_t dir)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    int ret = 0;
    int cb_bmp_tx = 0;
    int cb_bmp_rx = 0;
    int cb_bmp_echo = 0;
    lisa_i2s_priv_t *priv = (lisa_i2s_priv_t *)dev->priv_data;
    lisa_i2s_slot_mask_t slot_mask = priv->config.slot_mask;

    if ((dir & LISA_I2S_DIRECTION_RX) && (priv->rx_stream.state < LISA_I2S_STREAM_STATE_ENABLE)) {
        return LISA_DEVICE_OK;
    }

    if ((dir & LISA_I2S_DIRECTION_TX) && (priv->tx_stream.state < LISA_I2S_STREAM_STATE_ENABLE)) {
        return LISA_DEVICE_OK;
    }

    if (priv->config.echo.enable){
        LISA_LOGI(LOG_TAG, "I2S echo abort anyway");
        cb_bmp_echo = CH_BMP_STEREO;
    }

    if (dir & LISA_I2S_DIRECTION_TX) {
        cb_bmp_tx = slot_mask;
    }

    if (dir & LISA_I2S_DIRECTION_RX) {
        cb_bmp_rx = slot_mask;
    }

    if ((dir & LISA_I2S_DIRECTION_BOTH) || (cb_bmp_echo)) {
        ret = I2S_Abort_Channels(priv->hal_dev, cb_bmp_rx, cb_bmp_tx, cb_bmp_echo);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "I2S abort channels failed: %d", ret);
            return ret;
        }

        if (cb_bmp_tx) {
            priv->tx_stream.index = 0;
            xQueueReset(priv->tx_stream.queue);
            priv->tx_stream.state = LISA_I2S_STREAM_STATE_INIT;
        }

        if (cb_bmp_rx) {
            priv->rx_stream.index = 0;
            xQueueReset(priv->rx_stream.queue);
            priv->rx_stream.state = LISA_I2S_STREAM_STATE_INIT;
        }

        LISA_LOGI(LOG_TAG, "I2S %s %s stopped", 
                        cb_bmp_rx ? "RX" : "",
                        cb_bmp_tx ? "TX" : "");
    }
    
    return LISA_DEVICE_OK;
}

static int arcs_i2s_pause(lisa_device_t *dev, lisa_i2s_direction_t dir)
{
    return LISA_DEVICE_ERR_NOT_SUPPORT;
}

static int arcs_i2s_resume(lisa_device_t *dev, lisa_i2s_direction_t dir)
{
    return LISA_DEVICE_ERR_NOT_SUPPORT;
}

static int arcs_i2s_trigger(lisa_device_t *dev, lisa_i2s_direction_t dir, lisa_i2s_cmd_t cmd)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if ((dir & LISA_I2S_DIRECTION_BOTH) == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_i2s_priv_t *priv = (lisa_i2s_priv_t *)dev->priv_data;
    
    if (priv->state < LISA_I2S_STATE_CONFIGURED) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    switch (cmd) {
        case LISA_I2S_CMD_START:
            return arcs_i2s_start(dev, dir);
        case LISA_I2S_CMD_STOP:
            return arcs_i2s_stop(dev, dir);    // stop and disable interrupt
        case LISA_I2S_CMD_PAUSE:
            return arcs_i2s_pause(dev, dir);
        case LISA_I2S_CMD_RESUME:
            return arcs_i2s_resume(dev, dir);
        default:
            return LISA_DEVICE_ERR_INVALID;
    }
}

/* ===== 设备初始化函数 ===== */

#ifdef CONFIG_LISA_I2S0
static int arcs_i2s0_init(void)
{
    int ret = 0;
    lisa_i2s_priv_t *priv = &i2s0_priv;

    memset(priv, 0, sizeof(lisa_i2s_priv_t));

    priv->hal_dev = I2S0();
    priv->id = 0;

    lisa_i2s0_pinmux();

    LISA_LOGD(LOG_TAG, "I2S0 initialized");

    priv->state = LISA_I2S_STATE_IDLE;
    
    return LISA_DEVICE_OK;

}
#endif

#ifdef CONFIG_LISA_I2S1
static int arcs_i2s1_init(void)
{
    int ret = 0;
    lisa_i2s_priv_t *priv = &i2s1_priv;

    memset(priv, 0, sizeof(lisa_i2s_priv_t));

    priv->hal_dev = I2S1();
    priv->id = 1;

    lisa_i2s1_pinmux();

    LISA_LOGD(LOG_TAG, "I2S1 initialized");

    priv->state = LISA_I2S_STATE_IDLE;
    
    return LISA_DEVICE_OK;
}
#endif

/* ===== ARCS I2S API 实例 ===== */
static const lisa_i2s_api_t arcs_i2s_api = {
    .configure = arcs_i2s_configure,
    .get_config = arcs_i2s_get_config,
    .set_callback = arcs_i2s_set_callback,
    .write = arcs_i2s_write,
    .read = arcs_i2s_read,
    .trigger = arcs_i2s_trigger,
};

/* ===== 设备注册 ===== */
#ifdef CONFIG_LISA_I2S0
LISA_DEVICE_REGISTER(i2s0, &arcs_i2s_api, &i2s0_priv, NULL, arcs_i2s0_init, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#endif

#ifdef CONFIG_LISA_I2S1
LISA_DEVICE_REGISTER(i2s1, &arcs_i2s_api, &i2s1_priv, NULL, arcs_i2s1_init, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#endif
