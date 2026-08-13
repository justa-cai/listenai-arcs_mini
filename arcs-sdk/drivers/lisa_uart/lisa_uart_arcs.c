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
#include <soc/chip.h>
#include "dma.h"
#include "uart.h"
#include "cache.h"
#include "board.h"

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

#define LOG_TAG "lisa_uart_arcs"
#include <lisa_log.h>
/* 早期初始化阶段（PRE_SYSTEM_INIT）日志不可用，静默处理 */
#define UART_INIT_LOGE(tag, fmt, ...) LISA_LOGE(tag, fmt, ##__VA_ARGS__)

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
    volatile bool rx_paused;          /* 接收暂停标志: IDLE 中断时无空闲缓冲区,暂停接收等待消费 */
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
    volatile int rx_error;              /* 接收错误: 由中断回调记录，read_sync 返回 */
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

/* RX 错误事件统一在回调中恢复硬件和 HAL 状态 */
#define UART_RX_ERROR_EVENTS                                                                                           \
    (CSK_UART_EVENT_RX_OVERFLOW | CSK_UART_EVENT_RX_BREAK | CSK_UART_EVENT_RX_FRAMING_ERROR |                          \
     CSK_UART_EVENT_RX_PARITY_ERROR)

/* read_sync 等待循环的最大单次睡眠时间，用于兜底检查 HAL/FIFO 中的待处理数据 */
#define UART_RX_WAIT_POLL_MS 20U

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
 * @brief 恢复 UART RX 硬件和 HAL 接收状态
 *
 * 快速输入或错误中断后，驱动层 rx_busy、HAL rx_busy/xfer 和硬件 FIFO
 * 可能出现状态不一致。这里在驱动层做兜底清理，保证下一次 UART_Receive
 * 可以重新启动接收。
 */
static void uart_rx_hw_recover(lisa_uart_priv_t *priv)
{
    if (!priv || !priv->hal_handler) {
        return;
    }

    UART_RESOURCES *uart_res = (UART_RESOURCES *)priv->hal_handler;
    UART_RegDef *reg = uart_res->reg;

    /* 先中止 HAL 当前接收，避免继续使用旧的 rx_buf/rx_num 状态 */
    UART_Control(priv->hal_handler, CSK_UART_ABORT_RECEIVE, 1);

    /* 排空并复位 RX FIFO，避免残留字节继续触发旧状态 */
    uint32_t drain_count = 0;
    while (reg->REG_STATUS.bit.RX_FIFO_LEVEL > 0 && drain_count < UART_RX_FIFO_SIZE) {
        (void)reg->REG_RXTX_BUFFER.all;
        drain_count++;
    }
    reg->REG_CMD_SET.bit.RX_FIFO_RESET = 1;

    /* 清错误状态和 pending 中断，避免下一次使能 RX 后立即进入旧中断 */
    reg->REG_STATUS.all = UART_RX_OVERFLOW_ERR | UART_RX_PARITY_ERR | UART_RX_FRAMING_ERR | UART_RX_BREAK_INT;
    (void)reg->REG_IRQ_CAUSE.all;

    /* 同步清理 HAL 内部 RX 状态，避免 UART_Receive 误判 busy */
    uart_res->info->rx_status.rx_overflow = 0U;
    uart_res->info->rx_status.rx_framing_error = 0U;
    uart_res->info->rx_status.rx_parity_error = 0U;
    uart_res->info->rx_status.rx_break = 0U;
    uart_res->info->rx_status.rx_busy = 0U;
    uart_res->info->xfer.rx_num = 0U;
    uart_res->info->xfer.rx_cnt = 0U;
    uart_res->info->xfer.rx_buf = NULL;

    priv->rx_busy = false;
}

/**
 * @brief 启动一次 UART 接收
 *
 * UART_Receive 内部会使能 RX 中断，启动前先置驱动 busy。
 * 如果 HAL 仍认为 RX busy，说明上一次接收状态没有完全同步，恢复后重试一次。
 */
static int uart_rx_start(lisa_uart_priv_t *priv, uint8_t *buf, uint32_t len)
{
    int32_t ret;

    priv->rx_busy = true;
    ret = UART_Receive(priv->hal_handler, buf, len);
    if (ret == CSK_DRIVER_ERROR_BUSY) {
        LISA_LOGW(LOG_TAG, "UART_Receive busy, recover RX state and retry");
        uart_rx_hw_recover(priv);
        priv->rx_busy = true;
        ret = UART_Receive(priv->hal_handler, buf, len);
    }

    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "UART_Receive failed: %d", ret);
        priv->rx_busy = false;
        return (ret == CSK_DRIVER_ERROR_BUSY) ? LISA_DEVICE_ERR_BUSY : LISA_DEVICE_ERR_IO;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 从硬件 RX FIFO 读取已经到达的数据
 *
 * 非循环缓冲的中断模式下，数据可能已经进入 FIFO，但还没有触发一次完整的
 * UART_Receive 回调。read_sync 启动新接收前先读 FIFO，可以避免漏掉这部分数据。
 */
static uint32_t uart_rx_read_fifo(lisa_uart_priv_t *priv, uint8_t *buf, uint32_t len)
{
    UART_RESOURCES *uart_res = (UART_RESOURCES *)priv->hal_handler;
    UART_RegDef *reg = uart_res->reg;
    uint32_t fifo_level = reg->REG_STATUS.bit.RX_FIFO_LEVEL;
    uint32_t read_len = (fifo_level < len) ? fifo_level : len;

    for (uint32_t i = 0; i < read_len; i++) {
        buf[i] = (uint8_t)(reg->REG_RXTX_BUFFER.all & 0xFF);
    }

    return read_len;
}

/**
 * @brief 记录循环接收错误并唤醒等待中的 read_sync
 */
static void uart_rx_circular_report_error(lisa_uart_priv_t *priv, lisa_uart_rx_circular_buf_t *circ, int error)
{
    priv->rx_error = error;
    priv->rx_busy = false;

    if (!circ) {
        return;
    }

    circ->overflow = true;
    circ->enabled = false;
    circ->rx_paused = false;
    lisa_semaphore_give(circ->data_sem);
}

/**
 * @brief 完成当前循环缓冲区，并尝试启动下一个缓冲区接收
 */
static bool uart_rx_circular_complete_buffer(lisa_uart_priv_t *priv,
                                             lisa_uart_rx_circular_buf_t *circ,
                                             uint32_t rx_count,
                                             bool idle)
{
    if (rx_count == 0) {
        return false;
    }

    uint32_t active_idx = circ->active_idx;
    uint32_t next_idx = (active_idx + 1) % circ->buffer_count;

    circ->buffers_len[active_idx] = rx_count | (idle ? BUFFER_IDLE_FLAG : 0);

    if (circ->buffers_len[next_idx] != 0) {
        /* 下一个缓冲区仍未被应用层消费，暂停接收等待 read_sync 释放缓冲区 */
        circ->rx_paused = true;
        priv->rx_busy = false;
        lisa_semaphore_give(circ->data_sem);
        return true;
    }

    circ->active_idx = next_idx;

#if CONFIG_DCACHE_ENABLE
    if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
        dcache_invalidate_range((uint32_t)circ->buffers[next_idx],
                                (uint32_t)circ->buffers[next_idx] + circ->buffer_size);
    }
#endif

    if (uart_rx_start(priv, circ->buffers[next_idx], circ->buffer_size) != LISA_DEVICE_OK) {
        /* 无法继续启动接收时按溢出路径处理，保证应用层能被唤醒 */
        uart_rx_circular_report_error(priv, circ, LISA_DEVICE_ERR_OVERFLOW);
        return true;
    }

    lisa_semaphore_give(circ->data_sem);
    return true;
}

/**
 * @brief 兜底收集已到达但尚未通过回调提交的数据
 *
 * 快速输入时，HAL 计数或硬件 FIFO 里可能已经有数据，但 data_sem 尚未被给出。
 * read_sync 在短等待超时后调用本函数，避免一直睡到用户 timeout。
 */
static bool uart_rx_circular_collect_pending(lisa_uart_priv_t *priv, lisa_uart_rx_circular_buf_t *circ)
{
    if (!circ || !circ->enabled || circ->overflow || circ->rx_paused) {
        return false;
    }

    uint32_t active_idx = circ->active_idx;
    if (circ->buffers_len[active_idx] != 0) {
        /* 回调已经提交了当前缓冲区，回到 read_sync 主循环处理 */
        return true;
    }

    UART_RESOURCES *uart_res = (UART_RESOURCES *)priv->hal_handler;
    UART_RegDef *reg = uart_res->reg;
    uint32_t rx_count = UART_GetRxCount(priv->hal_handler);
    uint32_t fifo_level = reg->REG_STATUS.bit.RX_FIFO_LEVEL;

    if (rx_count == 0 && fifo_level == 0) {
        return false;
    }

    /* 停止本次 HAL 接收后，把 HAL 已计数和 FIFO 残留数据合并为一个缓冲区 */
    UART_Control(priv->hal_handler, CSK_UART_ABORT_RECEIVE, 1);

#if CONFIG_DCACHE_ENABLE
    if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA && rx_count > 0) {
        dcache_invalidate_range((uint32_t)circ->buffers[active_idx],
                                (uint32_t)circ->buffers[active_idx] + rx_count);
    }
#endif

    uint32_t total = (rx_count < circ->buffer_size) ? rx_count : circ->buffer_size;
    while (reg->REG_STATUS.bit.RX_FIFO_LEVEL > 0 && total < circ->buffer_size) {
        circ->buffers[active_idx][total++] = (uint8_t)(reg->REG_RXTX_BUFFER.all & 0xFF);
    }

    if (total == 0) {
        /* 只有 pending 状态但没有有效数据时，恢复当前缓冲区接收 */
        if (uart_rx_start(priv, circ->buffers[active_idx], circ->buffer_size) != LISA_DEVICE_OK) {
            uart_rx_circular_report_error(priv, circ, LISA_DEVICE_ERR_OVERFLOW);
            return true;
        }
        return false;
    }

    return uart_rx_circular_complete_buffer(priv, circ, total, true);
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
 * @brief 等待 TX FIFO 清空且移位寄存器发送完毕
 */
static void arcs_uart_wait_tx_complete(lisa_uart_priv_t *priv)
{
    UART_RESOURCES *uart_res = (UART_RESOURCES *)priv->hal_handler;
    UART_RegDef *uart_reg = uart_res->reg;
    while (uart_reg->REG_STATUS.bit.TX_ACTIVE);
}

/* ===== 设备反初始化 ===== */

/* HAL 层 PowerControl(OFF) + Uninitialize：清外设时钟门、IRQ、ISR 注册、HAL flags / cb_event / xfer。
 * 注意顺序：必须先 PowerControl(OFF)，再 Uninitialize；
 * PowerControl(OFF) 内部在清 POWERED 后会检查 INITIALIZED，先 Uninitialize 会让 PowerControl(OFF) 报错。 */
static void arcs_uart_teardown_hal(lisa_uart_priv_t *priv)
{
    UART_PowerControl(priv->hal_handler, CSK_POWER_OFF);
    UART_Uninitialize(priv->hal_handler);
}

/* 释放 configure() 阶段额外分配的 OS / 动态内存资源 */
static void arcs_uart_release_resources(lisa_uart_priv_t *priv)
{
    if (priv->rx_circ_buf) {
        uart_rx_circular_buf_free(priv->rx_circ_buf);
        priv->rx_circ_buf = NULL;
    }
    if (priv->tx_sem) {
        lisa_semaphore_delete(priv->tx_sem);
        priv->tx_sem = NULL;
    }
    if (priv->rx_sem) {
        lisa_semaphore_delete(priv->rx_sem);
        priv->rx_sem = NULL;
    }
    if (priv->tx_aligned_buf) {
        lisa_mem_free(priv->tx_aligned_buf);
        priv->tx_aligned_buf = NULL;
    }
}

/**
 * @brief 停止并释放单个 UART 实例的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 经各实例 deinit 包装调用。释放顺序与 _init / configure
 * 申请相反：
 *   1) HAL 下电：先 UART_PowerControl(OFF) 再 UART_Uninitialize；
 *   2) 释放 configure() 阶段分配的 rx_circ_buf / tx_sem / rx_sem / tx_aligned_buf；
 *   3) memset 整个 priv，回到 _init 之前的零初值。
 *
 * 约定：调用方需保证此时无收发在途、无并发业务在使用本设备（含 console / shell）。
 */
static int arcs_uart_deinit_instance(lisa_uart_priv_t *priv)
{
    if (priv == NULL) {
        return LISA_DEVICE_ERR_INVALID;
    }
    if (priv->hal_handler) {
        arcs_uart_teardown_hal(priv);
    }
    arcs_uart_release_resources(priv);
    memset(priv, 0, sizeof(*priv));
    return LISA_DEVICE_OK;
}

#ifdef CONFIG_LISA_UART0
static int arcs_uart0_deinit(void)
{
    return arcs_uart_deinit_instance(&uart0_priv);
}
#endif

#ifdef CONFIG_LISA_UART1
static int arcs_uart1_deinit(void)
{
    return arcs_uart_deinit_instance(&uart1_priv);
}
#endif

#ifdef CONFIG_LISA_UART2
static int arcs_uart2_deinit(void)
{
    return arcs_uart_deinit_instance(&uart2_priv);
}
#endif

#if CONFIG_LISA_PM
/* ===== System PM 回调 =====
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型：UART 控制器在
 * 睡眠时会掉电，由设备的持有方在睡眠前调 lisa_device_destroy(uartN) 释放全部软硬件
 * 资源（HAL 下电 + configure 资源），唤醒后调 lisa_device_reinit(uartN) 重建到 _init
 * 后的状态、再重新 configure()。因此驱动自身不再实现 prepare_suspend / resume_restore
 * （原先它们做 HAL 拆卸 / 重建，已被 destroy/reinit 覆盖）。
 *
 * 对作为 console / shell 后端的 UART，这一 destroy/reinit 编排由 system/console/
 * console_uart.c 的合成 PM 设备负责：睡前 flush + destroy，唤醒后 reinit + configure。
 *
 * check_idle：TX 在途阻止睡眠；常驻循环 RX 不算 busy（由上层 lisa_pm_lock 表达），
 * 仅当 rx_busy 且非循环 RX 时阻止睡眠。只读 priv，不取锁 / 不访问 HAL。
 */
static int32_t lisa_uart_pm_check_idle(void *ctx)
{
    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)ctx;
    lisa_uart_rx_circular_buf_t *rx_circ_buf;

    if (priv == NULL) {
        return 1;
    }
    if (priv->tx_busy) {
        return 0;
    }
    /* 循环 RX 是常驻能力，不算 busy；由上层 lisa_pm_lock_acquire/release 表达"不希望睡眠" */
    rx_circ_buf = priv->rx_circ_buf;
    if (priv->rx_busy && (rx_circ_buf == NULL || !rx_circ_buf->enabled)) {
        return 0;
    }
    return 1;
}

static const lisa_pm_system_ops_t arcs_uart_pm_ops = {
    .check_idle = lisa_uart_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore = NULL,
};
#endif /* CONFIG_LISA_PM */

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

    LISA_LOGD(LOG_TAG, "ISR event=0x%08X, enabled=%d, overflow=%d, active=%u, ready=%u",
              event, circ ? circ->enabled : -1, circ ? circ->overflow : -1,
              circ ? circ->active_idx : 0, circ ? circ->ready_idx : 0);

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
        LISA_LOGD(LOG_TAG, ">>> RECEIVE_COMPLETE <<<");

        if (circ && circ->enabled && !circ->overflow) {
            /* 获取接收长度 */
            uint32_t rx_count = UART_GetRxCount(priv->hal_handler);

            LISA_LOGD(LOG_TAG, "RX_COMP: cnt=%u, active=%u, ready=%u, buf0_len=%u, buf1_len=%u",
                      rx_count, circ->active_idx, circ->ready_idx,
                      circ->buffers_len[0], circ->buffers_len[1]);

            /* Cache invalidate (DMA 模式) */
#if CONFIG_DCACHE_ENABLE
            if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
                dcache_invalidate_range((uint32_t)circ->buffers[circ->active_idx],
                                        (uint32_t)circ->buffers[circ->active_idx] + rx_count);
            }
#endif

            uart_rx_circular_complete_buffer(priv, circ, rx_count, false);
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
        LISA_LOGD(LOG_TAG, ">>> IDLE TIMEOUT <<<");

        if (circ && circ->enabled && !circ->overflow) {
            /* 先获取已接收的字节数（在中止之前） */
            uint32_t rx_count = UART_GetRxCount(priv->hal_handler);

            LISA_LOGD(LOG_TAG, "IDLE: cnt=%u, active=%u, ready=%u, buf0_len=%u, buf1_len=%u",
                      rx_count, circ->active_idx, circ->ready_idx,
                      circ->buffers_len[0], circ->buffers_len[1]);

            if (rx_count > 0) {
                /*
                 * 有实际数据：中止当前接收，处理数据，然后启动下一个缓冲区。
                 */
                UART_Control(priv->hal_handler, CSK_UART_ABORT_RECEIVE, 1);

                /* Cache invalidate (DMA 模式) */
#if CONFIG_DCACHE_ENABLE
                if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
                    dcache_invalidate_range((uint32_t)circ->buffers[circ->active_idx],
                                            (uint32_t)circ->buffers[circ->active_idx] + rx_count);
                }
#endif

                uart_rx_circular_complete_buffer(priv, circ, rx_count, true);
            } else {
                /*
                 * rx_count == 0 的虚假超时中断（通常发生在 UART ENABLE 循环后）。
                 * HAL 的 IRQ handler 处理 RX_TIMEOUT 时，如果未达到 rx_num，
                 * 不会做 RECEIVE_COMPLETE，RX 中断仍然使能，rx_busy 仍为 1。
                 * 此时 HAL 仍处于正常的接收状态，无需任何操作。
                 * 如果 ABORT_RECEIVE + UART_Receive 重启，会因为 IRQ_CAUSE 中
                 * 残留的 RX_TIMEOUT 位导致 ISR 立即再次触发，形成无限循环。
                 */
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

check_rx_error:
    /* 错误中断统一恢复 RX 状态，并唤醒正在等待的 read_sync */
    if (event & UART_RX_ERROR_EVENTS) {
        int rx_error = (event & CSK_UART_EVENT_RX_OVERFLOW) ? LISA_DEVICE_ERR_OVERFLOW : LISA_DEVICE_ERR_IO;

        LISA_LOGW(LOG_TAG, "RX error event=0x%08X, recover receiver", event & UART_RX_ERROR_EVENTS);
        uart_rx_hw_recover(priv);
        priv->rx_error = rx_error;

        if (circ) {
            circ->overflow = true;
            circ->enabled = false;
            circ->rx_paused = false;
            lisa_semaphore_give(circ->data_sem);
        } else if (priv->rx_sem) {
            lisa_semaphore_give(priv->rx_sem);
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

    /* 创建发送和接收信号量（如果尚未创建）
     * 早期初始化阶段（heap 不可用）跳过信号量创建，poll_out 不依赖信号量 */
    if (!priv->tx_sem) {
        priv->tx_sem = lisa_semaphore_create(1);
    }

    if (!priv->rx_sem) {
        priv->rx_sem = lisa_semaphore_create(1);
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

        /* 分配新缓冲区（早期阶段 heap 不可用时跳过，后续可重新 configure） */
        priv->rx_circ_buf = uart_rx_circular_buf_alloc(&config->rx_buf_config);
        if (!priv->rx_circ_buf) {
            LISA_LOGW(LOG_TAG, "Failed to allocate rx circular buffers, rx disabled");
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
        int rx_start_ret;

        if (priv->rx_busy) {
            return LISA_DEVICE_ERR_BUSY;
        }

        if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_INTERRUPT) {
            /* 先取 FIFO 已有数据，避免重新 UART_Receive 时漏掉中断前到达的字节 */
            uint32_t fifo_read = uart_rx_read_fifo(priv, buf, len);

            if (fifo_read > 0) {
                return fifo_read;
            }

            UART_RESOURCES *uart_res = (UART_RESOURCES *)priv->hal_handler;
            if (uart_res->reg->REG_STATUS.all & (UART_RX_OVERFLOW_ERR | UART_RX_PARITY_ERR |
                                                 UART_RX_FRAMING_ERR | UART_RX_BREAK_INT)) {
                /* 硬件已进入错误状态时先恢复，否则后续 RX 中断可能无法重新触发 */
                int rx_error = (uart_res->reg->REG_STATUS.all & UART_RX_OVERFLOW_ERR) ?
                                   LISA_DEVICE_ERR_OVERFLOW :
                                   LISA_DEVICE_ERR_IO;
                uart_rx_hw_recover(priv);
                return rx_error;
            }
        }

        priv->rx_error = LISA_DEVICE_OK;

#if CONFIG_DCACHE_ENABLE
        if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
            CHECK_DMA_BUFFER_ALIGNMENT(buf, len, "read_sync");
            dcache_invalidate_range((uint32_t)buf, (uint32_t)buf + len);
        }
#endif

        lisa_semaphore_clear(priv->rx_sem);

        rx_start_ret = uart_rx_start(priv, buf, len);
        if (rx_start_ret != LISA_DEVICE_OK) {
            return rx_start_ret;
        }

        lisa_err_t sem_ret = lisa_semaphore_take(priv->rx_sem, timeout_ms);
        if (sem_ret != LISA_OK) {
            arcs_uart_read_abort(dev);
            return LISA_DEVICE_ERR_TIMEOUT;
        }

        if (priv->rx_error != LISA_DEVICE_OK) {
            /* 错误回调先唤醒 rx_sem，再由 read_sync 返回具体错误码 */
            int rx_error = priv->rx_error;
            priv->rx_error = LISA_DEVICE_OK;
            return rx_error;
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
        if (circ->overflow) {
            /* 溢出时缓冲区中可能仍有未读数据，先尝试读取 */
            if (circ->buffers_len[circ->ready_idx] == 0) {
                return LISA_DEVICE_ERR_OVERFLOW;
            }
            /* 有数据，继续进入读取循环 */
        } else {
            return LISA_DEVICE_ERR_NOT_READY;
        }
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

            LISA_LOGD(LOG_TAG, "[read_sync] buf %u has data: len=%u, idle=%d, offset=%u",
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
                uint32_t freed_idx = circ->ready_idx;
                circ->buffers_len[freed_idx] = 0;
                circ->read_offset = 0;

                /* 移动到下一个缓冲区 */
                circ->ready_idx = (circ->ready_idx + 1) % circ->buffer_count;

                /* 如果接收暂停且刚释放了缓冲区，重启 UART 接收 */
                if (circ->rx_paused) {
                    UART_RESOURCES *uart_res = (UART_RESOURCES *)priv->hal_handler;

                    /*
                     * 暂停期间 UART 硬件仍在接收数据到 FIFO。
                     * 检查是否发生了硬件 FIFO 溢出，如果是则清理。
                     */
                    if (uart_res->reg->REG_STATUS.all & UART_RX_OVERFLOW_ERR) {
                        /* FIFO 溢出：排空 FIFO 并重置 */
                        uint32_t drain = 0;
                        while (uart_res->reg->REG_STATUS.bit.RX_FIFO_LEVEL > 0 && drain < 128) {
                            (void)uart_res->reg->REG_RXTX_BUFFER.all;
                            drain++;
                        }
                        uart_res->reg->REG_CMD_SET.bit.RX_FIFO_RESET = 1;
                        uart_res->reg->REG_STATUS.all = UART_RX_OVERFLOW_ERR;
                        (void)uart_res->reg->REG_IRQ_CAUSE.all;
                        LISA_LOGD(LOG_TAG, "[read_sync] FIFO overflow during pause, drained %u bytes", drain);
                    }

                    circ->active_idx = freed_idx;
                    circ->rx_paused = false;
                    if (uart_rx_start(priv, circ->buffers[freed_idx], circ->buffer_size) != LISA_DEVICE_OK) {
                        uart_rx_circular_report_error(priv, circ, LISA_DEVICE_ERR_OVERFLOW);
                        return (total_read > 0) ? (int)total_read : LISA_DEVICE_ERR_OVERFLOW;
                    }
                    LISA_LOGD(LOG_TAG, "[read_sync] Resumed RX on buffer %u", freed_idx);
                }

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

        /* ===== 2. 当前 ready_idx 没有数据 ===== */

        /* 如果已溢出且数据已读完，返回已读数据或 OVERFLOW */
        if (circ->overflow) {
            LISA_LOGW(LOG_TAG, "[read_sync] Overflow, all data drained, total_read=%u", total_read);
            return (total_read > 0) ? (int)total_read : LISA_DEVICE_ERR_OVERFLOW;
        }

        LISA_LOGD(LOG_TAG, "[read_sync] No data in buf %u, waiting (total_read=%u, remaining=%u)",
                  circ->ready_idx, total_read, remaining);

        /* 计算剩余超时时间 */
        uint64_t elapsed_ms = lisa_os_get_tick_ms() - start_tick_ms;
        uint32_t remaining_timeout = (elapsed_ms >= timeout_ms) ? 0 : (timeout_ms - (uint32_t)elapsed_ms);

        /*
         * 不一次性睡完整个 timeout，短周期检查 HAL 计数和 FIFO。
         * 这样即使回调没有及时给 data_sem，read_sync 也能捞到已到达的数据。
         */
        uint32_t wait_timeout = (remaining_timeout > UART_RX_WAIT_POLL_MS) ? UART_RX_WAIT_POLL_MS : remaining_timeout;

        lisa_err_t sem_ret = lisa_semaphore_take(circ->data_sem, wait_timeout);
        if (sem_ret != LISA_OK) {
            if (uart_rx_circular_collect_pending(priv, circ)) {
                continue;
            }

            elapsed_ms = lisa_os_get_tick_ms() - start_tick_ms;
            if (elapsed_ms < timeout_ms) {
                continue;
            }

            LISA_LOGD(LOG_TAG, "[read_sync] Semaphore timeout, total_read=%u", total_read);
            return (total_read > 0) ? total_read : LISA_DEVICE_ERR_TIMEOUT;
        }

        /* 信号量获取成功，检查溢出和禁用状态 */
        if (circ->overflow) {
            /* 溢出时缓冲区中可能有未读数据，回到循环顶部继续读取 */
            LISA_LOGW(LOG_TAG, "[read_sync] Overflow detected, draining remaining data");
            continue;
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

static int arcs_uart_flush(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;

    if (!priv->configured) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    arcs_uart_wait_tx_complete(priv);

    return LISA_DEVICE_OK;
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
    priv->rx_error = LISA_DEVICE_OK;

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
        priv->rx_error = LISA_DEVICE_OK;
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
    circ->rx_paused = false;
    priv->rx_error = LISA_DEVICE_OK;

    /* 清空所有缓冲区的长度标记 */
    for (uint32_t i = 0; i < circ->buffer_count; i++) {
        circ->buffers_len[i] = 0;
    }

    /* 清空信号量 */
    lisa_semaphore_clear(circ->data_sem);

    /*
     * 清理 UART 硬件残留状态。
     *
     * 先中止 HAL 层接收操作，再直接操作寄存器清理硬件状态：
     * 中止接收 → 关中断 → 排空FIFO → 重置FIFO → 清错误 →
     * 清pending中断 → 确保ENABLE=1 → 清HAL状态。
     */
    {
        UART_RESOURCES *uart_res = (UART_RESOURCES *)priv->hal_handler;
        UART_RegDef *reg = uart_res->reg;

        /* 中止 HAL 层当前接收操作，清除 rx_num/rx_cnt/rx_buf 等内部状态 */
        UART_Control(priv->hal_handler, CSK_UART_ABORT_RECEIVE, 1);

        /* 关闭所有 RX 中断 */
        reg->REG_IRQ_MASK.all &= ~(UART_RX_DATA_AVAILABLE
                                  | UART_RX_TIMEOUT
                                  | UART_RX_LINE_ERR
                                  | UART_RX_DMA_DONE
                                  | UART_RX_DMA_TIMEOUT);

        /* 排空 RX FIFO */
        uint32_t drain_count = 0;
        while (reg->REG_STATUS.bit.RX_FIFO_LEVEL > 0 && drain_count < 128) {
            (void)reg->REG_RXTX_BUFFER.all;
            drain_count++;
        }

        /* FIFO 重置 */
        reg->REG_CMD_SET.bit.RX_FIFO_RESET = 1;

        /* 清除错误状态位 */
        reg->REG_STATUS.all = UART_RX_OVERFLOW_ERR
                            | UART_TX_OVERFLOW_ERR
                            | UART_RX_PARITY_ERR
                            | UART_RX_FRAMING_ERR
                            | UART_RX_BREAK_INT;

        /* 读取 IRQ_CAUSE 消除所有 pending 中断 */
        (void)reg->REG_IRQ_CAUSE.all;

        /* 确保 UART ENABLE 位为 1（rx_disable 关闭 RX 后 ENABLE 可能为 0） */
        if (!reg->REG_CTRL.bit.ENABLE) {
            reg->REG_CTRL.bit.ENABLE = 1;
            uart_res->info->flags |= UART_FLAG_TX_ENABLED;
        }

        /* 清除 HAL 层的 RX 状态 */
        uart_res->info->rx_status.rx_overflow = 0U;
        uart_res->info->rx_status.rx_framing_error = 0U;
        uart_res->info->rx_status.rx_parity_error = 0U;
        uart_res->info->rx_status.rx_break = 0U;
        uart_res->info->rx_status.rx_busy = 0U;

        /* 清除 HAL 层的接收计数，防止残留数据触发虚假完成事件 */
        uart_res->info->xfer.rx_num = 0U;
        uart_res->info->xfer.rx_cnt = 0U;
        uart_res->info->xfer.rx_buf = NULL;
    }

    /*
     * 必须在 UART_Receive() 之前设置 enabled=true 和 rx_busy=true，
     * 因为 UART_Receive() 会立即使能 RX 中断。如果新数据很快到达，
     * ISR 会在 UART_Receive() 返回前触发。ISR 检查 circ->enabled
     * 来决定走循环缓冲区路径还是旧路径。
     */
    circ->enabled = true;
    priv->rx_busy = true;

    /* Cache invalidate (DMA 模式) */
#if CONFIG_DCACHE_ENABLE
    if (priv->current_config.transfer_mode == LISA_UART_TRANSFER_MODE_DMA) {
        dcache_invalidate_range((uint32_t)circ->buffers[0], (uint32_t)circ->buffers[0] + circ->buffer_size);
    }
#endif

    /* 启动第一个缓冲区接收 */
    int ret = uart_rx_start(priv, circ->buffers[0], circ->buffer_size);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to start UART receive: %d", ret);
        circ->enabled = false;
        priv->rx_busy = false;
        return ret;
    }

    /* 使能 RX */
    ret = UART_Control(priv->hal_handler, CSK_UART_CONTROL_RX, 1);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "Failed to enable UART RX: %d", ret);
        circ->enabled = false;
        priv->rx_busy = false;
        return LISA_DEVICE_ERR_IO;
    }

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
        circ->rx_paused = false;

        /* 释放信号量，避免 read_sync 永久阻塞 */
        lisa_semaphore_give(circ->data_sem);

        LISA_LOGD(LOG_TAG, "RX circular buffer disabled");
    }

    priv->rx_busy = false;
    priv->rx_error = LISA_DEVICE_OK;

    return (ret == CSK_DRIVER_OK) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_IO;
}

/**
 * @brief 等待 UART RX 线路空闲
 *
 * 排空所有循环缓冲区中已接收的数据，等待线路空闲。
 * 通过检查 BUFFER_IDLE_FLAG 来判断数据是否在空闲中断触发后到达。
 * 检测到空闲后等待 10ms 保护窗口确保无新数据，再返回成功。
 *
 * 用于 Flash 擦写前确保 modem 在途数据已全部收入缓冲区，
 * 避免 erase 期间中断被全局禁用时丢失数据。
 *
 * @param dev        UART 设备指针
 * @param timeout_ms 最大等待时间（毫秒）
 * @return LISA_DEVICE_OK 成功检测到空闲，或 LISA_DEVICE_ERR_TIMEOUT 超时
 */
static int arcs_uart_rx_wait_idle(lisa_device_t *dev, uint32_t timeout_ms)
{
    lisa_uart_priv_t *priv = (lisa_uart_priv_t *)dev->priv_data;
    lisa_uart_rx_circular_buf_t *circ = priv->rx_circ_buf;

    if (!circ || !circ->enabled) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    uint32_t start_ms = lisa_os_get_tick_ms();
    bool idle_seen = false;
    bool any_data_consumed = false;
    uint32_t guard_start_ms = 0;
    uint8_t drain_buf[256];

    while (1) {
        uint32_t elapsed = lisa_os_get_tick_ms() - start_ms;
        if (elapsed >= timeout_ms) {
            return idle_seen ? LISA_DEVICE_OK : LISA_DEVICE_ERR_TIMEOUT;
        }

        /* 检查当前 ready 缓冲区是否有数据 */
        uint32_t buf_flag = circ->buffers_len[circ->ready_idx];
        if (buf_flag != 0) {
            any_data_consumed = true;
            uint32_t buf_len = buf_flag & BUFFER_LEN_MASK;
            bool is_idle = (buf_flag & BUFFER_IDLE_FLAG) != 0;

            /* 消费缓冲区中的数据（丢弃，调用方不关心里面的内容） */
            uint32_t available = buf_len - circ->read_offset;
            uint32_t copy_len = (available < sizeof(drain_buf)) ? available : sizeof(drain_buf);
            memcpy(drain_buf, circ->buffers[circ->ready_idx] + circ->read_offset, copy_len);
            circ->read_offset += copy_len;

            /* 缓冲区是否完全消费完 */
            if (circ->read_offset >= buf_len) {
                uint32_t freed_idx = circ->ready_idx;
                circ->buffers_len[freed_idx] = 0;
                circ->read_offset = 0;
                circ->ready_idx = (circ->ready_idx + 1) % circ->buffer_count;

                /* 如果 rx_paused，恢复 RX 到刚释放的缓冲区 */
                if (circ->rx_paused) {
                    UART_RESOURCES *uart_res = (UART_RESOURCES *)priv->hal_handler;

                    /* 检查暂停期间 FIFO 是否溢出 */
                    if (uart_res->reg->REG_STATUS.all & UART_RX_OVERFLOW_ERR) {
                        uint32_t drain = 0;
                        while (uart_res->reg->REG_STATUS.bit.RX_FIFO_LEVEL > 0 && drain < 128) {
                            (void)uart_res->reg->REG_RXTX_BUFFER.all;
                            drain++;
                        }
                        uart_res->reg->REG_CMD_SET.bit.RX_FIFO_RESET = 1;
                        uart_res->reg->REG_STATUS.all = UART_RX_OVERFLOW_ERR;
                        (void)uart_res->reg->REG_IRQ_CAUSE.all;
                        LISA_LOGW(LOG_TAG, "[rx_wait_idle] FIFO overflow during pause, drained %u bytes", drain);
                    }

                    circ->active_idx = freed_idx;
                    circ->rx_paused = false;
                    if (uart_rx_start(priv, circ->buffers[freed_idx], circ->buffer_size) != LISA_DEVICE_OK) {
                        uart_rx_circular_report_error(priv, circ, LISA_DEVICE_ERR_OVERFLOW);
                        return LISA_DEVICE_ERR_OVERFLOW;
                    }
                }

                /* 如果刚消费的 buffer 是空闲中断触发的，记录空闲检测 */
                if (is_idle) {
                    idle_seen = true;
                    guard_start_ms = lisa_os_get_tick_ms();
                }
            }
        } else {
            /* 当前 ready 缓冲区无数据 */
            if (circ->overflow) {
                return LISA_DEVICE_ERR_OVERFLOW;
            }

            /* 进入函数至今没有消费过任何数据：UART 本来就处于空闲状态。
             * 等待 20ms 确认没有新数据到达（data_sem 没有被 post）后直接返回。 */
            if (!any_data_consumed && elapsed >= 20) {
                return LISA_DEVICE_OK;
            }

            if (idle_seen) {
                /* 验证保护期：空闲后持续 10ms 无新数据才算真正空闲 */
                if ((lisa_os_get_tick_ms() - guard_start_ms) >= 10) {
                    return LISA_DEVICE_OK;
                }
            }
        }

        /* 短暂阻塞避免忙等，用 data_sem 等待（有新数据立即唤醒） */
        lisa_semaphore_take(circ->data_sem, 10);
    }
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
        UART_INIT_LOGE(LOG_TAG, "Failed to get UART0 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL UART，注册事件回调 */
    if (UART_Initialize(uart0_priv.hal_handler, uart_hal_event_callback, &uart0_priv) != CSK_DRIVER_OK) {
        UART_INIT_LOGE(LOG_TAG, "Failed to initialize UART0");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 上电 */
    if (UART_PowerControl(uart0_priv.hal_handler, CSK_POWER_FULL) != CSK_DRIVER_OK) {
        UART_INIT_LOGE(LOG_TAG, "Failed to power on UART0");
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
        UART_INIT_LOGE(LOG_TAG, "Failed to get UART1 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL UART，注册事件回调 */
    if (UART_Initialize(uart1_priv.hal_handler, uart_hal_event_callback, &uart1_priv) != CSK_DRIVER_OK) {
        UART_INIT_LOGE(LOG_TAG, "Failed to initialize UART1");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 上电 */
    if (UART_PowerControl(uart1_priv.hal_handler, CSK_POWER_FULL) != CSK_DRIVER_OK) {
        UART_INIT_LOGE(LOG_TAG, "Failed to power on UART1");
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
        UART_INIT_LOGE(LOG_TAG, "Failed to get UART2 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL UART，注册事件回调 */
    if (UART_Initialize(uart2_priv.hal_handler, uart_hal_event_callback, &uart2_priv) != CSK_DRIVER_OK) {
        UART_INIT_LOGE(LOG_TAG, "Failed to initialize UART2");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 上电 */
    if (UART_PowerControl(uart2_priv.hal_handler, CSK_POWER_FULL) != CSK_DRIVER_OK) {
        UART_INIT_LOGE(LOG_TAG, "Failed to power on UART2");
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
    .flush = arcs_uart_flush,
    .rx_enable = arcs_uart_rx_enable,
    .rx_disable = arcs_uart_rx_disable,
#ifdef CONFIG_LISA_UART_ASYNC_API
    .write_async = arcs_uart_write_async,
    .set_callback = arcs_uart_set_callback,
    .write_abort = arcs_uart_write_abort,
    .get_tx_count = arcs_uart_get_tx_count,
#endif
    .rx_wait_idle = arcs_uart_rx_wait_idle,
};

/* ===== 设备注册 ===== */
/* 当 console UART 后端启用时，所有 UART 使用 EARLY 级别以支持早期初始化 */
#ifdef CONFIG_CONSOLE_UART_EARLY_INIT
#define UART_INIT_LEVEL  LISA_DEVICE_LEVEL_EARLY
#define UART_INIT_PRIO   LISA_DEVICE_PRIORITY_CRITICAL
#else
#define UART_INIT_LEVEL  LISA_DEVICE_LEVEL_NORMAL
#define UART_INIT_PRIO   LISA_DEVICE_PRIORITY_NORMAL
#endif

#ifdef CONFIG_LISA_UART0
LISA_DEVICE_REGISTER_DEINIT(uart0, &arcs_uart_api, &uart0_priv, NULL, arcs_uart0_init,
                            arcs_uart0_deinit, UART_INIT_LEVEL, UART_INIT_PRIO);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(uart0, &arcs_uart_pm_ops, NULL, &uart0_priv);
#endif
#endif
#ifdef CONFIG_LISA_UART1
LISA_DEVICE_REGISTER_DEINIT(uart1, &arcs_uart_api, &uart1_priv, NULL, arcs_uart1_init,
                            arcs_uart1_deinit, UART_INIT_LEVEL, UART_INIT_PRIO);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(uart1, &arcs_uart_pm_ops, NULL, &uart1_priv);
#endif
#endif
#ifdef CONFIG_LISA_UART2
LISA_DEVICE_REGISTER_DEINIT(uart2, &arcs_uart_api, &uart2_priv, NULL, arcs_uart2_init,
                            arcs_uart2_deinit, UART_INIT_LEVEL, UART_INIT_PRIO);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(uart2, &arcs_uart_pm_ops, NULL, &uart2_priv);
#endif
#endif
