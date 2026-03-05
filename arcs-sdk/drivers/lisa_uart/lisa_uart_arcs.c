/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_uart_arcs.c
 * @brief LISA UART ARCS 平台适配层
 *
 * 此文件实现 ARCS 芯片平台的 UART 硬件适配
 */

#include "lisa_uart.h"
#include "Driver_UART.h"
#include <stddef.h>
#include <string.h>
#include <lisa_semaphore.h>
#include <lisa_mem.h>
#include <lisa_time.h>
#include "arcs_ap.h"
#include "dma.h"
#include "uart.h"
#include "cache.h"
#include "board.h"

#define LOG_TAG "lisa_uart_arcs"
#include <lisa_log.h>

/* DMA 缓冲区对齐检查宏 */
#define IS_DMA_BUFFER_ALIGNED(buf, len) \
    (((uint32_t)(buf) & (HAL_DCACHE_CFG_LINE_SIZE - 1)) == 0 && \
     ((len) & (HAL_DCACHE_CFG_LINE_SIZE - 1)) == 0)

#define CHECK_DMA_BUFFER_ALIGNMENT(buf, len, op_name)                                                                  \
    do {                                                                                                               \
        if (((uint32_t)(buf) & (HAL_DCACHE_CFG_LINE_SIZE - 1)) != 0) {                                                 \
            LISA_LOGW(LOG_TAG, "%s: Buffer address 0x%08x is not %d-byte aligned for DMA operation", op_name,          \
                      (uint32_t)(buf), HAL_DCACHE_CFG_LINE_SIZE);                                                      \
        }                                                                                                              \
        if (((len) & (HAL_DCACHE_CFG_LINE_SIZE - 1)) != 0) {                                                           \
            LISA_LOGW(LOG_TAG, "%s: Buffer length %u is not %d-byte aligned for DMA operation", op_name,               \
                      (uint32_t)(len), HAL_DCACHE_CFG_LINE_SIZE);                                                      \
        }                                                                                                              \
    } while (0)

/* ===== UART 循环接收缓冲区运行时状态 ===== */

/* buffers_len 数组标志位定义 */
#define BUFFER_IDLE_FLAG  0x80000000  /* 最高位标记是否由空闲中断触发 */
#define BUFFER_LEN_MASK   0x7FFFFFFF  /* 低31位为实际数据长度 */

/**
 * @brief UART 循环接收缓冲区运行时状态
 */
typedef struct {
    uint8_t **buffers;                /* 缓冲区指针数组 */
    uint32_t buffer_count;            /* 缓冲区数量 */
    uint32_t buffer_size;             /* 单个缓冲区大小 */

    /* DMA/INT 接收状态 */
    volatile uint32_t active_idx;     /* 当前 DMA/INT 正在写入的缓冲区索引 */

    /* 应用层读取状态 */
    volatile uint32_t ready_idx;      /* 应用层正在读取的缓冲区索引 */
    volatile uint32_t read_offset;    /* 应用层当前缓冲区已读取的偏移量 */

    /* 每个缓冲区的元数据 */
    volatile uint32_t *buffers_len;   /* 每个缓冲区的有效数据长度，0表示未接收或已读完
                                          * 最高位(bit31)标记是否由空闲中断触发
                                          * 低31位为实际数据长度 */

    /* 同步机制 */
    lisa_semaphore_t *data_sem;       /* 数据就绪信号量 */

    /* 状态标志 */
    volatile bool enabled;            /* 接收是否已启用 */
    volatile bool overflow;           /* 溢出标志: true 表示缓冲区已满,驱动已停止接收 */
} lisa_uart_rx_circular_buf_t;

/* ===== UART 设备私有数据 ===== */
typedef struct {
    void *hal_handler;                 /* HAL UART 句柄 (UART0/UART1/UART2) */
    lisa_uart_config_t current_config; /* 当前配置 */
    lisa_uart_callback_t callback;     /* 用户回调函数 */
    void *user_data;                   /* 用户数据 */
    lisa_semaphore_t *tx_sem;          /* 发送完成信号量 */
    lisa_semaphore_t *rx_sem;          /* 接收完成信号量 */
    volatile bool tx_busy;             /* 发送忙标志 */
    volatile bool rx_busy;             /* 接收忙标志 */
    volatile bool configured;          /* 是否已配置标志 */

    /* ===== 新增: 循环接收缓冲区 ===== */
    lisa_uart_rx_circular_buf_t *rx_circ_buf;  /* 循环接收缓冲区运行时状态 */

    /* ===== 新增: 发送对齐缓冲区 ===== */
    uint8_t *tx_aligned_buf;           /* 发送对齐缓冲区指针 (用于异步发送) */
} lisa_uart_priv_t;

/* ===== UART 设备静态实例 ===== */
#ifdef CONFIG_LISA_UART0
static lisa_uart_priv_t uart0_priv;
#endif
#ifdef CONFIG_LISA_UART1
static lisa_uart_priv_t uart1_priv;
#endif
#ifdef CONFIG_LISA_UART2
static lisa_uart_priv_t uart2_priv;
#endif

/* ===== 内部辅助函数 ===== */
static int arcs_uart_write_abort(lisa_device_t *dev);
static int arcs_uart_read_abort(lisa_device_t *dev);

/**
 * @brief 分配循环接收缓冲区 (Cache Line 对齐)
 */
static lisa_uart_rx_circular_buf_t *uart_rx_circular_buf_alloc(const lisa_uart_rx_buf_config_t *config)
{
    if (!config || config->buffer_count == 0 || config->buffer_size == 0) {
        return NULL;
    }

    lisa_uart_rx_circular_buf_t *circ = lisa_mem_alloc(sizeof(lisa_uart_rx_circular_buf_t));
    if (!circ) {
        LISA_LOGE(LOG_TAG, "Failed to allocate circular buffer structure");
        return NULL;
    }
    memset(circ, 0, sizeof(lisa_uart_rx_circular_buf_t));

    /* 分配缓冲区指针数组 */
    circ->buffers = lisa_mem_alloc(sizeof(uint8_t *) * config->buffer_count);
    if (!circ->buffers) {
        LISA_LOGE(LOG_TAG, "Failed to allocate buffer pointer array");
        lisa_mem_free(circ);
        return NULL;
    }
    memset(circ->buffers, 0, sizeof(uint8_t *) * config->buffer_count);

    /* 计算 Cache Line 对齐的缓冲区大小 */
    uint32_t aligned_size = (config->buffer_size + HAL_DCACHE_CFG_LINE_SIZE - 1) & ~(HAL_DCACHE_CFG_LINE_SIZE - 1);

    /* 分配对齐的缓冲区 */
    for (uint32_t i = 0; i < config->buffer_count; i++) {
        circ->buffers[i] = lisa_mem_align_alloc(HAL_DCACHE_CFG_LINE_SIZE, aligned_size);
        if (!circ->buffers[i]) {
            LISA_LOGE(LOG_TAG, "Failed to allocate aligned buffer %u", i);
            /* 分配失败，释放已分配的 */
            for (uint32_t j = 0; j < i; j++) {
                lisa_mem_free(circ->buffers[j]);
            }
            lisa_mem_free(circ->buffers);
            lisa_mem_free(circ);
            return NULL;
        }
        memset(circ->buffers[i], 0, aligned_size);
    }

    circ->buffer_count = config->buffer_count;
    circ->buffer_size = aligned_size;

    /* 分配 buffers_len 数组 */
    circ->buffers_len = lisa_mem_alloc(sizeof(uint32_t) * config->buffer_count);
    if (!circ->buffers_len) {
        LISA_LOGE(LOG_TAG, "Failed to allocate buffers_len array");
        for (uint32_t i = 0; i < config->buffer_count; i++) {
            lisa_mem_free(circ->buffers[i]);
        }
        lisa_mem_free(circ->buffers);
        lisa_mem_free(circ);
        return NULL;
    }
    memset((void *)circ->buffers_len, 0, sizeof(uint32_t) * config->buffer_count);

    /* 创建数据就绪信号量 */
    circ->data_sem = lisa_semaphore_create(1);
    if (!circ->data_sem) {
        LISA_LOGE(LOG_TAG, "Failed to create data semaphore");
        lisa_mem_free((void *)circ->buffers_len);
        for (uint32_t i = 0; i < config->buffer_count; i++) {
            lisa_mem_free(circ->buffers[i]);
        }
        lisa_mem_free(circ->buffers);
        lisa_mem_free(circ);
        return NULL;
    }

    LISA_LOGI(LOG_TAG, "Allocated %u circular buffers, each %u bytes (aligned)", config->buffer_count, aligned_size);
    return circ;
}

/**
 * @brief 释放循环接收缓冲区
 */
static void uart_rx_circular_buf_free(lisa_uart_rx_circular_buf_t *circ)
{
    if (!circ) {
        return;
    }

    if (circ->buffers) {
        for (uint32_t i = 0; i < circ->buffer_count; i++) {
            if (circ->buffers[i]) {
                lisa_mem_free(circ->buffers[i]);
            }
        }
        lisa_mem_free(circ->buffers);
    }

    if (circ->buffers_len) {
        lisa_mem_free((void *)circ->buffers_len);
    }

    if (circ->data_sem) {
        lisa_semaphore_delete(circ->data_sem);
    }

    lisa_mem_free(circ);
    LISA_LOGI(LOG_TAG, "Freed circular buffers");
}

/**
 * @brief 将 LISA UART 事件转换为 HAL UART 事件
 */
static uint32_t lisa_event_to_hal_event(lisa_uart_event_t event)
{
    uint32_t hal_event = 0;

    if (event & LISA_UART_EVENT_TX_DONE) {
        hal_event |= CSK_UART_EVENT_SEND_COMPLETE;
    }
    if (event & LISA_UART_EVENT_RX_READY) {
        hal_event |= CSK_UART_EVENT_RECEIVE_COMPLETE;
    }
    if (event & LISA_UART_EVENT_RX_TIMEOUT) {
        hal_event |= CSK_UART_EVENT_RX_TIMEOUT;
    }
    if (event & LISA_UART_EVENT_ERROR) {
        hal_event |= CSK_UART_EVENT_RX_OVERFLOW;
    }
    if (event & LISA_UART_EVENT_BREAK) {
        hal_event |= CSK_UART_EVENT_RX_BREAK;
    }
    if (event & LISA_UART_EVENT_OVERRUN) {
        hal_event |= CSK_UART_EVENT_RX_OVERFLOW;
    }
    if (event & LISA_UART_EVENT_PARITY_ERROR) {
        hal_event |= CSK_UART_EVENT_RX_PARITY_ERROR;
    }
    if (event & LISA_UART_EVENT_FRAME_ERROR) {
        hal_event |= CSK_UART_EVENT_RX_FRAMING_ERROR;
    }

    return hal_event;
}

/**
 * @brief 将 HAL UART 事件转换为 LISA UART 事件
 */
static lisa_uart_event_t hal_event_to_lisa_event(uint32_t hal_event)
{
    lisa_uart_event_t event = 0;

    if (hal_event & CSK_UART_EVENT_SEND_COMPLETE) {
        event |= LISA_UART_EVENT_TX_DONE;
    }
    if (hal_event & CSK_UART_EVENT_RECEIVE_COMPLETE) {
        event |= LISA_UART_EVENT_RX_READY;
    }
    if (hal_event & CSK_UART_EVENT_RX_TIMEOUT) {
        event |= LISA_UART_EVENT_RX_TIMEOUT;
    }
    if (hal_event & CSK_UART_EVENT_TX_OVERFLOW) {
        event |= LISA_UART_EVENT_ERROR;
    }
    if (hal_event & CSK_UART_EVENT_RX_OVERFLOW) {
        event |= LISA_UART_EVENT_OVERRUN;
    }
    if (hal_event & CSK_UART_EVENT_RX_BREAK) {
        event |= LISA_UART_EVENT_BREAK;
    }
    if (hal_event & CSK_UART_EVENT_RX_FRAMING_ERROR) {
        event |= LISA_UART_EVENT_FRAME_ERROR;
    }
    if (hal_event & CSK_UART_EVENT_RX_PARITY_ERROR) {
        event |= LISA_UART_EVENT_PARITY_ERROR;
    }

    return event;
}

/**
 * @brief HAL UART 事件回调函数
 */
static void uart_hal_event_callback(uint32_t event, void *workspace)
{
    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)workspace;
    if (!priv) {
        return;
    }

    lisa_uart_rx_circular_buf_t *circ = priv->rx_circ_buf;

    LISA_LOGD(LOG_TAG, "uart event callback: 0x%08X, circ=%p, enabled=%d, overflow=%d",
              event, circ, circ ? circ->enabled : -1, circ ? circ->overflow : -1);

    /* ===== 发送完成中断 ===== */
    if (event & CSK_UART_EVENT_SEND_COMPLETE) {
        priv->tx_busy = false;

        /* 释放发送对齐缓冲区 (如果存在) */
        if (priv->tx_aligned_buf) {
            lisa_mem_free(priv->tx_aligned_buf);
            priv->tx_aligned_buf = NULL;
            LISA_LOGD(LOG_TAG, "Freed tx aligned buffer in callback");
        }

        if (priv->tx_sem) {
            lisa_semaphore_give(priv->tx_sem);
        }
    }

    /* ===== 接收完成中断 (缓冲区满) ===== */
    if (event & CSK_UART_EVENT_RECEIVE_COMPLETE) {
        LISA_LOGD(LOG_TAG, ">>> RECEIVE_COMPLETE EVENT TRIGGERED <<<");

        if (circ && circ->enabled && !circ->overflow) {
            /* 获取接收长度 */
            uint32_t rx_count = UART_GetRxCount(priv->hal_handler);

            LISA_LOGD(LOG_TAG, "RECEIVE_COMPLETE: rx_count=%u, active_idx=%u, ready_idx=%u",
                      rx_count, circ->active_idx, circ->ready_idx);

            /* Cache invalidate (DMA 模式) */
#if CONFIG_DCACHE_ENABLE
            if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
                dcache_invalidate_range((uint32_t)circ->buffers[circ->active_idx],
                                        (uint32_t)circ->buffers[circ->active_idx] + rx_count);
            }
#endif

            /* 记录当前 active_idx 缓冲区的接收长度 (不带空闲标志) */
            circ->buffers_len[circ->active_idx] = rx_count;

            /* 计算下一个要使用的缓冲区索引 */
            uint32_t next_idx = (circ->active_idx + 1) % circ->buffer_count;

            /* 检查溢出: 下一个缓冲区是否还有未读数据 */
            if (circ->buffers_len[next_idx] != 0) {
                /* 缓冲区溢出: 下一个缓冲区还有数据未读,应用层读取太慢 */
                LISA_LOGW(LOG_TAG, "!!! OVERFLOW in RECEIVE_COMPLETE: next_idx=%u has %u bytes unread (active_idx=%u, rx_count=%u)",
                          next_idx, circ->buffers_len[next_idx] & BUFFER_LEN_MASK, circ->active_idx, rx_count);
                UART_Control(priv->hal_handler, CSK_UART_ABORT_RECEIVE, 1);
                circ->overflow = true;
                circ->enabled = false;
                priv->rx_busy = false;
                LISA_LOGW(LOG_TAG, "RX buffer overflow, reception stopped");
                lisa_semaphore_give(circ->data_sem);
                goto user_callback;
            }

            LISA_LOGD(LOG_TAG, "RECEIVE_COMPLETE: No overflow, switching to next buffer");

            /* 切换到下一个缓冲区 */
            circ->active_idx = next_idx;

#if CONFIG_DCACHE_ENABLE
            if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
                dcache_invalidate_range((uint32_t)circ->buffers[next_idx],
                                        (uint32_t)circ->buffers[next_idx] + circ->buffer_size);
            }
#endif

            /* 启动下一个缓冲区接收 */
            UART_Receive(priv->hal_handler, circ->buffers[next_idx], circ->buffer_size);

            /* 通知应用层有新数据 */
            lisa_semaphore_give(circ->data_sem);
        } else {
            /* 没有使用循环缓冲区，使用原有逻辑 */
            priv->rx_busy = false;
            if (priv->rx_sem) {
                lisa_semaphore_give(priv->rx_sem);
            }
        }
    }

    /* ===== 空闲中断 (不定长数据) ===== */
    if (event & CSK_UART_EVENT_RX_TIMEOUT) {
        LISA_LOGD(LOG_TAG, ">>> IDLE TIMEOUT EVENT TRIGGERED <<<");

        if (circ && circ->enabled && !circ->overflow) {
            /* 先获取已接收的字节数（在中止之前） */
            uint32_t rx_count = UART_GetRxCount(priv->hal_handler);

            LISA_LOGD(LOG_TAG, "Before ABORT: rx_count=%u", rx_count);

            /* 中止当前接收 */
            UART_Control(priv->hal_handler, CSK_UART_ABORT_RECEIVE, 1);

            LISA_LOGD(LOG_TAG, "=== IDLE INT START ===");
            LISA_LOGD(LOG_TAG, "rx_count=%u, active_idx=%u, ready_idx=%u",
                      rx_count, circ->active_idx, circ->ready_idx);

            if (rx_count > 0) {
                LISA_LOGD(LOG_TAG, "rx_count > 0, checking overflow condition");

                /* Cache invalidate (DMA 模式) */
#if CONFIG_DCACHE_ENABLE
                if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
                    dcache_invalidate_range((uint32_t)circ->buffers[circ->active_idx],
                                            (uint32_t)circ->buffers[circ->active_idx] + rx_count);
                }
#endif

                /* 记录当前 active_idx 缓冲区的接收长度，并设置空闲标志 */
                circ->buffers_len[circ->active_idx] = rx_count | BUFFER_IDLE_FLAG;

                /* 计算下一个要使用的缓冲区索引 */
                uint32_t next_idx = (circ->active_idx + 1) % circ->buffer_count;

                /* 检查溢出: 下一个缓冲区是否还有未读数据 */
                if (circ->buffers_len[next_idx] != 0) {
                    /* 缓冲区溢出: 下一个缓冲区还有数据未读,应用层读取太慢 */
                    LISA_LOGW(LOG_TAG, "!!! OVERFLOW in RX_TIMEOUT: next_idx=%u has %u bytes unread (active_idx=%u, rx_count=%u)",
                              next_idx, circ->buffers_len[next_idx] & BUFFER_LEN_MASK, circ->active_idx, rx_count);
                    circ->overflow = true;
                    circ->enabled = false;
                    priv->rx_busy = false;
                    LISA_LOGW(LOG_TAG, "RX buffer overflow, reception stopped");
                    lisa_semaphore_give(circ->data_sem);
                    goto user_callback;
                }

                LISA_LOGD(LOG_TAG, "No overflow, switching to next buffer");

                /* 切换到下一个缓冲区 */
                circ->active_idx = next_idx;

#if CONFIG_DCACHE_ENABLE
                if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
                    dcache_invalidate_range((uint32_t)circ->buffers[next_idx],
                                            (uint32_t)circ->buffers[next_idx] + circ->buffer_size);
                }
#endif

                LISA_LOGD(LOG_TAG, "Switching to next buffer: next_idx=%u", next_idx);

                /* 启动下一个缓冲区接收 */
                UART_Receive(priv->hal_handler, circ->buffers[next_idx], circ->buffer_size);

                LISA_LOGD(LOG_TAG, "Giving semaphore to wake up read_sync");
                /* 通知应用层有新数据 */
                lisa_semaphore_give(circ->data_sem);

                LISA_LOGD(LOG_TAG, "=== IDLE INT END ===");
            }
        } else {
            /* 没有使用循环缓冲区，使用原有逻辑 */
            LISA_LOGD(LOG_TAG, "IDLE INT: using old logic (no circ buffer), circ=%p, enabled=%d, overflow=%d",
                      circ, circ ? circ->enabled : -1, circ ? circ->overflow : -1);
            priv->rx_busy = false;
            if (priv->rx_sem) {
                lisa_semaphore_give(priv->rx_sem);
            }
        }
    }

user_callback:
    /* 调用用户回调 */
    if (priv->callback) {
        lisa_uart_event_t lisa_event = hal_event_to_lisa_event(event);
        priv->callback(lisa_event, priv->user_data);
    }
}

/* ===== ARCS平台UART实现函数 ===== */

static int arcs_uart_configure(lisa_device_t *dev, const lisa_uart_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;
    uint32_t control = 0;
    int32_t ret;

    /* 检查是否正在传输数据 */
    if (priv->tx_busy) {
        LISA_LOGW(LOG_TAG, "Cannot configure UART while TX is in progress");
        return LISA_DEVICE_ERR_BUSY;
    }

    if (priv->rx_busy) {
        LISA_LOGW(LOG_TAG, "Cannot configure UART while RX is in progress");
        return LISA_DEVICE_ERR_BUSY;
    }

    /* 配置传输模式（中断或DMA） */
    if (config->transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
        control |= CSK_UART_Function_CONTROL_Dma;
    } else {
        control |= CSK_UART_Function_CONTROL_Int;
    }

    /* 如果配置了循环缓冲区，使用带超时的异步模式（启用空闲中断） */
    if (config->rx_buf_config.buffer_count > 0 && config->rx_buf_config.buffer_size > 0) {
        control |= CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT;
    } else {
        control |= CSK_UART_MODE_ASYNCHRONOUS;
    }

    /* 配置数据位 */
    switch (config->data_bits) {
    case LISA_UART_DATA_BITS_5:
        control |= CSK_UART_DATA_BITS_5;
        break;
    case LISA_UART_DATA_BITS_6:
        control |= CSK_UART_DATA_BITS_6;
        break;
    case LISA_UART_DATA_BITS_7:
        control |= CSK_UART_DATA_BITS_7;
        break;
    case LISA_UART_DATA_BITS_8:
        control |= CSK_UART_DATA_BITS_8;
        break;
    default:
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 配置校验位 */
    switch (config->parity) {
    case LISA_UART_PARITY_NONE:
        control |= CSK_UART_PARITY_NONE;
        break;
    case LISA_UART_PARITY_ODD:
        control |= CSK_UART_PARITY_ODD;
        break;
    case LISA_UART_PARITY_EVEN:
        control |= CSK_UART_PARITY_EVEN;
        break;
    default:
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 配置停止位 */
    switch (config->stop_bits) {
    case LISA_UART_STOP_BITS_1:
        control |= CSK_UART_STOP_BITS_1;
        break;
    case LISA_UART_STOP_BITS_1_5:
        control |= CSK_UART_STOP_BITS_1_5;
        break;
    case LISA_UART_STOP_BITS_2:
        control |= CSK_UART_STOP_BITS_2;
        break;
    default:
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 配置流控制 */
    switch (config->flow_ctrl) {
    case LISA_UART_FLOW_CONTROL_NONE:
        control |= CSK_UART_FLOW_CONTROL_NONE;
        break;
    case LISA_UART_FLOW_CONTROL_RTS_CTS:
        control |= CSK_UART_FLOW_CONTROL_RTS_CTS;
        break;
    default:
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    /* 使用默认GPIO配置 */
    control |= CSK_UART_GPIO_CONTROL_DEFAULT;

    /* 配置 DMA 通道（如果是 DMA 模式） */
    if (config->transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
        /* 验证 TX DMA 通道有效性 (0-3 或 0xFF) */
        if (config->dma_tx_channel != 0xFF && config->dma_tx_channel > 3) {
            LISA_LOGE(LOG_TAG, "Invalid DMA TX channel: %d (valid range: 0-3 or 0xFF for auto)", config->dma_tx_channel);
            return LISA_DEVICE_ERR_INVALID;
        }

        /* 验证 RX DMA 通道有效性 (0-3 或 0xFF) */
        if (config->dma_rx_channel != 0xFF && config->dma_rx_channel > 3) {
            LISA_LOGE(LOG_TAG, "Invalid DMA RX channel: %d (valid range: 0-3 or 0xFF for auto)", config->dma_rx_channel);
            return LISA_DEVICE_ERR_INVALID;
        }

        /* 配置 TX DMA 通道 */
        ret = UART_SetDMATxChannel(priv->hal_handler, config->dma_tx_channel);
        if (ret != CSK_DRIVER_OK) {
            LISA_LOGE(LOG_TAG, "UART_SetDMATxChannel failed: %d (channel=%d)", ret, config->dma_tx_channel);
            return LISA_DEVICE_ERR_IO;
        }
        LISA_LOGI(LOG_TAG, "DMA TX channel set to: %d", config->dma_tx_channel);

        /* 配置 RX DMA 通道 */
        ret = UART_SetDMARxChannel(priv->hal_handler, config->dma_rx_channel);
        if (ret != CSK_DRIVER_OK) {
            LISA_LOGE(LOG_TAG, "UART_SetDMARxChannel failed: %d (channel=%d)", ret, config->dma_rx_channel);
            return LISA_DEVICE_ERR_IO;
        }
        LISA_LOGI(LOG_TAG, "DMA RX channel set to: %d", config->dma_rx_channel);
    }

    /* 调用 HAL 配置函数，波特率作为参数传递 */
    ret = UART_Control(priv->hal_handler, control, config->baudrate);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "UART_Control failed: %d", ret);
        return LISA_DEVICE_ERR_IO;
    }

    /* 默认使能发送 */
    UART_Control(priv->hal_handler, CSK_UART_CONTROL_TX, 1);

    /* 创建发送和接收信号量（如果尚未创建） */
    if (!priv->tx_sem) {
        priv->tx_sem = lisa_semaphore_create(1);
        if (!priv->tx_sem) {
            LISA_LOGE(LOG_TAG, "Failed to create tx semaphore");
            return LISA_DEVICE_ERR_NO_MEM;
        }
    }

    if (!priv->rx_sem) {
        priv->rx_sem = lisa_semaphore_create(1);
        if (!priv->rx_sem) {
            LISA_LOGE(LOG_TAG, "Failed to create rx semaphore");
            return LISA_DEVICE_ERR_NO_MEM;
        }
    }

    /* 配置循环接收缓冲区 */
    if (config->rx_buf_config.buffer_count > 0 && config->rx_buf_config.buffer_size > 0) {
        /* 释放旧缓冲区 */
        if (priv->rx_circ_buf) {
            /* 先禁用接收 */
            if (priv->rx_circ_buf->enabled) {
                UART_Control(priv->hal_handler, CSK_UART_CONTROL_RX, 0);
                UART_Control(priv->hal_handler, CSK_UART_ABORT_RECEIVE, 1);
                priv->rx_circ_buf->enabled = false;
                priv->rx_busy = false;
            }
            uart_rx_circular_buf_free(priv->rx_circ_buf);
            priv->rx_circ_buf = NULL;
        }

        /* 分配新缓冲区 */
        priv->rx_circ_buf = uart_rx_circular_buf_alloc(&config->rx_buf_config);
        if (!priv->rx_circ_buf) {
            LISA_LOGE(LOG_TAG, "Failed to allocate rx circular buffers");
            return LISA_DEVICE_ERR_NO_MEM;
        }
    }

    /* 保存配置 */
    memcpy(&priv->current_config, config, sizeof(lisa_uart_config_t));

    /* 标记为已配置 */
    priv->configured = true;

    return LISA_DEVICE_OK;
}

static int arcs_uart_get_config(lisa_device_t *dev, lisa_uart_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    memcpy(config, &priv->current_config, sizeof(lisa_uart_config_t));

    return LISA_DEVICE_OK;
}

static int arcs_uart_write_sync(lisa_device_t *dev, const uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    if (!lisa_device_is_initialized(dev) || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    /* 检查是否已配置 */
    if (!priv->configured) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    int32_t ret;
    uint8_t *aligned_buf = NULL;
    const uint8_t *send_buf = buf;
    uint32_t aligned_len = len;

    if (priv->tx_busy) {
        return LISA_DEVICE_ERR_BUSY;
    }

    priv->tx_busy = true;

    /* 兜底策略：如果之前的 tx_aligned_buf 未被释放，先释放它，避免内存泄漏 */
    if (priv->tx_aligned_buf) {
        LISA_LOGW(LOG_TAG, "tx_aligned_buf not freed in previous transfer, freeing now");
        lisa_mem_free(priv->tx_aligned_buf);
        priv->tx_aligned_buf = NULL;
    }

    /* DMA模式下需要刷新Cache，确保内存数据同步到主存 */
#if CONFIG_DCACHE_ENABLE
    if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
        /* 检查缓冲区是否对齐 */
        if (!IS_DMA_BUFFER_ALIGNED(buf, len)) {
            /* 缓冲区未对齐，分配对齐内存 */
            aligned_len = (len + HAL_DCACHE_CFG_LINE_SIZE - 1) & ~(HAL_DCACHE_CFG_LINE_SIZE - 1);
            aligned_buf = lisa_mem_align_alloc(HAL_DCACHE_CFG_LINE_SIZE, aligned_len);
            if (!aligned_buf) {
                LISA_LOGE(LOG_TAG, "Failed to allocate aligned buffer for write_sync");
                priv->tx_busy = false;
                return LISA_DEVICE_ERR_NO_MEM;
            }

            /* 复制数据到对齐缓冲区 */
            memcpy(aligned_buf, buf, len);
            send_buf = aligned_buf;

            LISA_LOGD(LOG_TAG, "write_sync: Using aligned buffer (addr=0x%08x, len=%u)",
                      (uint32_t)aligned_buf, aligned_len);
        }

        dcache_flush_range((uint32_t)send_buf, (uint32_t)send_buf + aligned_len);
    }
#endif

    /* 清空信号量，避免旧信号残留 */
    lisa_semaphore_clear(priv->tx_sem);

    /* 启动硬件发送 */
    ret = UART_Send(priv->hal_handler, send_buf, len);
    if (ret != CSK_DRIVER_OK) {
        priv->tx_busy = false;
        if (aligned_buf) {
            lisa_mem_free(aligned_buf);
        }
        return LISA_DEVICE_ERR_IO;
    }

    /* 保存对齐缓冲区指针，在发送完成回调中释放 */
    if (aligned_buf) {
        priv->tx_aligned_buf = aligned_buf;
    }

    /* 等待发送完成信号量 */
    lisa_err_t sem_ret = lisa_semaphore_take(priv->tx_sem, timeout_ms);
    if (sem_ret != LISA_OK) {
        /* 超时或失败，中止发送 */
        arcs_uart_write_abort(dev);
        /* 注意: aligned_buf 会在 write_abort 或中断回调中释放，这里不再手动释放 */
        return LISA_DEVICE_ERR_TIMEOUT;
    }

    /* 获取实际发送的字节数 */
    uint32_t tx_count = UART_GetTxCount(priv->hal_handler);

    /* 注意: aligned_buf 已在发送完成中断回调中释放，这里不再手动释放 */

    return tx_count;
}

static int arcs_uart_read_sync(lisa_device_t *dev, uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    if (!lisa_device_is_initialized(dev) || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    /* 检查是否已配置 */
    if (!priv->configured) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_uart_rx_circular_buf_t *circ = priv->rx_circ_buf;

    /* 如果没有使用循环缓冲区，使用原有实现 */
    if (!circ) {
        /* ===== 原有实现（不使用循环缓冲区） ===== */
        int32_t ret;

        if (priv->rx_busy) {
            return LISA_DEVICE_ERR_BUSY;
        }

        priv->rx_busy = true;

#if CONFIG_DCACHE_ENABLE
        if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
            CHECK_DMA_BUFFER_ALIGNMENT(buf, len, "read_sync");
            dcache_invalidate_range((uint32_t)buf, (uint32_t)buf + len);
        }
#endif

        lisa_semaphore_clear(priv->rx_sem);

        ret = UART_Receive(priv->hal_handler, buf, len);
        if (ret != CSK_DRIVER_OK) {
            priv->rx_busy = false;
            return LISA_DEVICE_ERR_IO;
        }

        lisa_err_t sem_ret = lisa_semaphore_take(priv->rx_sem, timeout_ms);
        if (sem_ret != LISA_OK) {
            arcs_uart_read_abort(dev);
            return LISA_DEVICE_ERR_TIMEOUT;
        }

        uint32_t rx_count = UART_GetRxCount(priv->hal_handler);

#if CONFIG_DCACHE_ENABLE
        if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
            dcache_invalidate_range((uint32_t)buf, (uint32_t)buf + rx_count);
        }
#endif

        return rx_count;
    }

    /* ===== 使用循环缓冲区的新实现 ===== */

    /* 检查接收是否已使能 */
    if (!circ->enabled) {
        /* 检查是否是溢出导致的禁用 */
        if (circ->overflow) {
            return LISA_DEVICE_ERR_OVERFLOW;
        }
        return LISA_DEVICE_ERR_NOT_READY;
    }

    uint32_t total_read = 0;
    uint32_t remaining = len;
    uint64_t start_tick_ms = lisa_os_get_tick_ms();  /* 记录开始时间(毫秒) */

    while (remaining > 0) {
        /* ===== 1. 先检查当前 ready_idx 缓冲区是否有数据 ===== */
        uint32_t buf_len_with_flag = circ->buffers_len[circ->ready_idx];

        if (buf_len_with_flag != 0) {
            /* 当前缓冲区有数据，提取实际长度和空闲标志 */
            bool is_idle = (buf_len_with_flag & BUFFER_IDLE_FLAG) != 0;
            uint32_t buf_len = buf_len_with_flag & BUFFER_LEN_MASK;

            LISA_LOGD(LOG_TAG, "[read_sync] Buffer %u has data: len=%u, is_idle=%d, read_offset=%u",
                      circ->ready_idx, buf_len, is_idle, circ->read_offset);

            /* 计算可读取长度 */
            uint32_t available = buf_len - circ->read_offset;
            uint32_t copy_len = (remaining < available) ? remaining : available;

            /* 复制数据 */
            memcpy(buf + total_read, circ->buffers[circ->ready_idx] + circ->read_offset, copy_len);

            total_read += copy_len;
            remaining -= copy_len;
            circ->read_offset += copy_len;

            /* 检查当前缓冲区是否读完 */
            if (circ->read_offset >= buf_len) {
                LISA_LOGD(LOG_TAG, "[read_sync] Buffer %u exhausted, is_idle=%d, total_read=%u",
                          circ->ready_idx, is_idle, total_read);

                /* 清空当前缓冲区（中断可以重新使用） */
                circ->buffers_len[circ->ready_idx] = 0;
                circ->read_offset = 0;

                /* 移动到下一个缓冲区 */
                circ->ready_idx = (circ->ready_idx + 1) % circ->buffer_count;

                /* 如果是空闲中断触发的数据，立即返回 */
                if (is_idle) {
                    LISA_LOGD(LOG_TAG, "[read_sync] Idle interrupt data, returning %u bytes", total_read);
                    return total_read;
                }

                /* 如果已读满指定长度，返回 */
                if (remaining == 0) {
                    LISA_LOGD(LOG_TAG, "[read_sync] Read complete, returning %u bytes", total_read);
                    return total_read;
                }

                /* 继续循环，检查下一个缓冲区 */
            } else {
                /* 缓冲区还有数据，但用户要的数据已读满 */
                LISA_LOGD(LOG_TAG, "[read_sync] Read complete (buffer partial), returning %u bytes", total_read);
                return total_read;
            }

            continue;  /* 重新进入循环，检查下一个缓冲区 */
        }

        /* ===== 2. 当前 ready_idx 没有数据，等待信号量 ===== */
        LISA_LOGD(LOG_TAG, "[read_sync] No data in buffer %u, waiting for semaphore (total_read=%u, remaining=%u)",
                  circ->ready_idx, total_read, remaining);

        /* 计算剩余超时时间 */
        uint64_t elapsed_ms = lisa_os_get_tick_ms() - start_tick_ms;
        uint32_t remaining_timeout = (elapsed_ms >= timeout_ms) ? 0 : (timeout_ms - (uint32_t)elapsed_ms);

        lisa_err_t sem_ret = lisa_semaphore_take(circ->data_sem, remaining_timeout);
        if (sem_ret != LISA_OK) {
            /* 超时 */
            LISA_LOGD(LOG_TAG, "[read_sync] Semaphore timeout, total_read=%u", total_read);
            return (total_read > 0) ? total_read : LISA_DEVICE_ERR_TIMEOUT;
        }

        /* 信号量获取成功，检查溢出和禁用状态 */
        if (circ->overflow) {
            LISA_LOGW(LOG_TAG, "[read_sync] Overflow detected");
            return LISA_DEVICE_ERR_OVERFLOW;
        }

        if (!circ->enabled) {
            LISA_LOGD(LOG_TAG, "[read_sync] RX disabled, returning %u bytes", total_read);
            return total_read;
        }

        /* 重新进入循环，检查是否有新数据 */
    }

    LISA_LOGD(LOG_TAG, "[read_sync] Complete, returning %u bytes", total_read);
    return total_read;
}

static int arcs_uart_poll_in(lisa_device_t *dev, uint8_t *byte)
{
    if (!lisa_device_is_initialized(dev) || !byte) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    /* 检查是否已配置 */
    if (!priv->configured) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 检查是否配置了循环缓冲区模式 */
    if (priv->rx_circ_buf != NULL) {
        /* 循环缓冲区模式下,硬件中断/DMA会自动搬运FIFO数据到缓冲区,
         * poll_in无法从FIFO读取数据,请使用read_sync代替 */
        LISA_LOGW(LOG_TAG, "poll_in is not supported when circular buffer is enabled, use read_sync instead");
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    UART_RESOURCES *uart_res = (UART_RESOURCES *)priv->hal_handler;
    UART_RegDef *uart_reg = uart_res->reg;

    if (uart_reg->REG_STATUS.bit.RX_FIFO_LEVEL > 0) {
        *byte = (uint8_t)(uart_reg->REG_RXTX_BUFFER.all & 0xFF);
        return LISA_DEVICE_OK;
    }

    return LISA_DEVICE_ERR_TIMEOUT;
}

static void arcs_uart_poll_out(lisa_device_t *dev, uint8_t byte)
{
    if (!lisa_device_is_initialized(dev)) {
        return;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    /* 检查是否已配置 */
    if (!priv->configured) {
        return;
    }

    UART_RESOURCES *uart_res = (UART_RESOURCES *)priv->hal_handler;
    UART_RegDef *uart_reg = uart_res->reg;

    uart_reg->REG_RXTX_BUFFER.all = byte;
    while (!uart_reg->REG_STATUS.bit.TX_FIFO_SPACE);
}

static int arcs_uart_write_abort(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    /* 检查是否已配置 */
    if (!priv->configured) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 调用 HAL 终止发送函数 */
    int32_t ret = UART_Control(priv->hal_handler, CSK_UART_ABORT_SEND, 1);
    if (ret != CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_IO;
    }

    priv->tx_busy = false;

    /* 释放发送对齐缓冲区 (如果存在) */
    if (priv->tx_aligned_buf) {
        lisa_mem_free(priv->tx_aligned_buf);
        priv->tx_aligned_buf = NULL;
        LISA_LOGD(LOG_TAG, "Freed tx aligned buffer in write_abort");
    }

    return LISA_DEVICE_OK;
}

static int arcs_uart_read_abort(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    /* 检查是否已配置 */
    if (!priv->configured) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 调用 HAL 终止接收函数 */
    int32_t ret = UART_Control(priv->hal_handler, CSK_UART_ABORT_RECEIVE, 1);
    if (ret != CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_IO;
    }

    priv->rx_busy = false;

    return LISA_DEVICE_OK;
}

static int arcs_uart_rx_enable(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    /* 检查是否已配置 */
    if (!priv->configured) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_uart_rx_circular_buf_t *circ = priv->rx_circ_buf;

    /* 如果没有配置循环缓冲区，使用原有逻辑 */
    if (!circ) {
        int32_t ret = UART_Control(priv->hal_handler, CSK_UART_CONTROL_RX, 1);
        return (ret == CSK_DRIVER_OK) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_IO;
    }

    if (circ->enabled) {
        return LISA_DEVICE_OK;  /* 已经启用 */
    }

    /* 重置状态 */
    circ->active_idx = 0;
    circ->ready_idx = 0;
    circ->read_offset = 0;
    circ->overflow = false;

    /* 清空所有缓冲区的长度标记 */
    for (uint32_t i = 0; i < circ->buffer_count; i++) {
        circ->buffers_len[i] = 0;
    }

    /* 清空信号量 */
    lisa_semaphore_clear(circ->data_sem);

    /* Cache invalidate (DMA 模式) */
#if CONFIG_DCACHE_ENABLE
    if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
        dcache_invalidate_range((uint32_t)circ->buffers[0], (uint32_t)circ->buffers[0] + circ->buffer_size);
    }
#endif

    /* 启动第一个缓冲区接收 */
    int32_t ret = UART_Receive(priv->hal_handler, circ->buffers[0], circ->buffer_size);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "Failed to start UART receive: %d", ret);
        return LISA_DEVICE_ERR_IO;
    }

    /* 使能 RX */
    ret = UART_Control(priv->hal_handler, CSK_UART_CONTROL_RX, 1);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "Failed to enable UART RX: %d", ret);
        return LISA_DEVICE_ERR_IO;
    }

    circ->enabled = true;
    priv->rx_busy = true;

    LISA_LOGI(LOG_TAG, "RX circular buffer enabled");
    return LISA_DEVICE_OK;
}

static int arcs_uart_rx_disable(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    /* 检查是否已配置 */
    if (!priv->configured) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_uart_rx_circular_buf_t *circ = priv->rx_circ_buf;

    /* 停止接收 */
    int32_t ret = UART_Control(priv->hal_handler, CSK_UART_CONTROL_RX, 0);

    if (circ) {
        /* 中止当前接收操作 */
        UART_Control(priv->hal_handler, CSK_UART_ABORT_RECEIVE, 1);

        circ->enabled = false;
        circ->overflow = false;

        /* 释放信号量，避免 read_sync 永久阻塞 */
        lisa_semaphore_give(circ->data_sem);

        LISA_LOGI(LOG_TAG, "RX circular buffer disabled");
    }

    priv->rx_busy = false;

    return (ret == CSK_DRIVER_OK) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_IO;
}

#ifdef CONFIG_LISA_UART_ASYNC_API
static int arcs_uart_set_callback(lisa_device_t *dev, lisa_uart_callback_t callback, void *user_data)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    priv->callback = callback;
    priv->user_data = user_data;

    return LISA_DEVICE_OK;
}

static int arcs_uart_write_async(lisa_device_t *dev, const uint8_t *buf, uint32_t len)
{
    if (!lisa_device_is_initialized(dev) || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    /* 检查是否已配置 */
    if (!priv->configured) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    int32_t ret;
    uint8_t *aligned_buf = NULL;
    const uint8_t *send_buf = buf;
    uint32_t aligned_len = len;

    if (priv->tx_busy) {
        return LISA_DEVICE_ERR_BUSY;
    }

    priv->tx_busy = true;

    /* 兜底策略：如果之前的 tx_aligned_buf 未被释放，先释放它，避免内存泄漏 */
    if (priv->tx_aligned_buf) {
        LISA_LOGW(LOG_TAG, "tx_aligned_buf not freed in previous transfer, freeing now");
        lisa_mem_free(priv->tx_aligned_buf);
        priv->tx_aligned_buf = NULL;
    }

    /* DMA模式下需要刷新Cache，确保内存数据同步到主存 */
#if CONFIG_DCACHE_ENABLE
    if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
        /* 检查缓冲区是否对齐 */
        if (!IS_DMA_BUFFER_ALIGNED(buf, len)) {
            /* 缓冲区未对齐，分配对齐内存 */
            aligned_len = (len + HAL_DCACHE_CFG_LINE_SIZE - 1) & ~(HAL_DCACHE_CFG_LINE_SIZE - 1);
            aligned_buf = lisa_mem_align_alloc(HAL_DCACHE_CFG_LINE_SIZE, aligned_len);
            if (!aligned_buf) {
                LISA_LOGE(LOG_TAG, "Failed to allocate aligned buffer for write_async");
                priv->tx_busy = false;
                return LISA_DEVICE_ERR_NO_MEM;
            }

            /* 复制数据到对齐缓冲区 */
            memcpy(aligned_buf, buf, len);
            send_buf = aligned_buf;

            LISA_LOGD(LOG_TAG, "write_async: Using aligned buffer (addr=0x%08x, len=%u)",
                      (uint32_t)aligned_buf, aligned_len);
        }

        dcache_flush_range((uint32_t)send_buf, (uint32_t)send_buf + aligned_len);
    }
#endif

    ret = UART_Send(priv->hal_handler, send_buf, len);
    if (ret != CSK_DRIVER_OK) {
        priv->tx_busy = false;
        if (aligned_buf) {
            lisa_mem_free(aligned_buf);
        }
        return LISA_DEVICE_ERR_IO;
    }

    /* 保存对齐缓冲区指针，在发送完成回调中释放 */
    if (aligned_buf) {
        priv->tx_aligned_buf = aligned_buf;
    }

    return len;
}

static uint32_t arcs_uart_get_tx_count(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return 0;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    /* 检查是否已配置 */
    if (!priv->configured) {
        return 0;
    }

    return UART_GetTxCount(priv->hal_handler);
}
#endif

/* ===== 设备初始化函数 ===== */
#ifdef CONFIG_LISA_UART0
static int arcs_uart0_init(void)
{
    /* 清空私有数据 */
    memset(&uart0_priv, 0, sizeof(lisa_uart_priv_t));

    /* 获取 HAL UART0 句柄 */
    uart0_priv.hal_handler = UART0();
    if (!uart0_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get UART0 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL UART，注册事件回调 */
    if (UART_Initialize(uart0_priv.hal_handler, uart_hal_event_callback, &uart0_priv) != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "Failed to initialize UART0");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 上电 */
    if (UART_PowerControl(uart0_priv.hal_handler, CSK_POWER_FULL) != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "Failed to power on UART0");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    lisa_uart0_pinmux();

    return LISA_DEVICE_OK;
}
#endif

#ifdef CONFIG_LISA_UART1
static int arcs_uart1_init(void)
{
    /* 清空私有数据 */
    memset(&uart1_priv, 0, sizeof(lisa_uart_priv_t));

    /* 获取 HAL UART1 句柄 */
    uart1_priv.hal_handler = UART1();
    if (!uart1_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get UART1 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL UART，注册事件回调 */
    if (UART_Initialize(uart1_priv.hal_handler, uart_hal_event_callback, &uart1_priv) != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "Failed to initialize UART1");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 上电 */
    if (UART_PowerControl(uart1_priv.hal_handler, CSK_POWER_FULL) != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "Failed to power on UART1");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    lisa_uart1_pinmux();

    return LISA_DEVICE_OK;
}
#endif

#ifdef CONFIG_LISA_UART2
static int arcs_uart2_init(void)
{
    /* 清空私有数据 */
    memset(&uart2_priv, 0, sizeof(lisa_uart_priv_t));

    /* 获取 HAL UART2 句柄 */
    uart2_priv.hal_handler = UART2();
    if (!uart2_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get UART2 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL UART，注册事件回调 */
    if (UART_Initialize(uart2_priv.hal_handler, uart_hal_event_callback, &uart2_priv) != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "Failed to initialize UART2");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 上电 */
    if (UART_PowerControl(uart2_priv.hal_handler, CSK_POWER_FULL) != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "Failed to power on UART2");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    lisa_uart2_pinmux();

    return LISA_DEVICE_OK;
}
#endif

/* ===== ARCS UART API 实例 ===== */
static const lisa_uart_api_t arcs_uart_api = {
    .configure = arcs_uart_configure,
    .get_config = arcs_uart_get_config,
    .write_sync = arcs_uart_write_sync,
    .read_sync = arcs_uart_read_sync,
    .poll_in = arcs_uart_poll_in,
    .poll_out = arcs_uart_poll_out,
    .rx_enable = arcs_uart_rx_enable,
    .rx_disable = arcs_uart_rx_disable,
#ifdef CONFIG_LISA_UART_ASYNC_API
    .write_async = arcs_uart_write_async,
    .set_callback = arcs_uart_set_callback,
    .write_abort = arcs_uart_write_abort,
    .get_tx_count = arcs_uart_get_tx_count,
#endif
};

/* ===== 设备注册 ===== */
/* 注意:不要轻易修改设备名称(uart0/uart1/uart2),终端(console)会依赖这些名称 */
#ifdef CONFIG_LISA_UART0
LISA_DEVICE_REGISTER(uart0, &arcs_uart_api, &uart0_priv, NULL, arcs_uart0_init, LISA_DEVICE_PRIORITY_NORMAL);
#endif
#ifdef CONFIG_LISA_UART1
LISA_DEVICE_REGISTER(uart1, &arcs_uart_api, &uart1_priv, NULL, arcs_uart1_init, LISA_DEVICE_PRIORITY_NORMAL);
#endif
#ifdef CONFIG_LISA_UART2
LISA_DEVICE_REGISTER(uart2, &arcs_uart_api, &uart2_priv, NULL, arcs_uart2_init, LISA_DEVICE_PRIORITY_NORMAL);
#endif
