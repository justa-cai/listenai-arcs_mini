/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <string.h>
#include "lisa_device.h"
#include "lisa_rgb.h"

#include "Driver_RGB.h"
#include "Driver_GPDMA.h"
#include "lisa_semaphore.h"
#include "lisa_mutex.h"
#include "lisa_mem.h"
#include "sysheap.h"
#include "board.h"

#define LOG_TAG "lisa_rgb"
#include "lisa_log.h"

/* ========================================================================
 * 私有数据结构
 * ======================================================================== */

typedef struct {
    void *hal_rgb;                     /**< HAL RGB 驱动句柄 */
    lisa_mutex_t *lock;                /**< 互斥锁 */
    lisa_semaphore_t *done_sem;        /**< 传输完成信号量 */
    lisa_semaphore_t *write_sem;       /**< 写入信号量（防止重复写入） */

    /* GPDMA 配置 */
    csk_gpdma_ch_t gpdma_ch;           /**< GPDMA 通道号 */

    /* 帧缓冲配置 */
    uint8_t *fb_buf[2];                /**< 双缓冲：[0]=写缓冲, [1]=显示缓冲（或反之） */
    uint32_t fb_size;                  /**< 单个帧缓冲区大小（字节） */

    /* Bounce Buffer 配置 */
    uint8_t *bounce_buffer[2];         /**< 内部RAM bounce buffer（Ping/Pong） */
    uint32_t bb_size;                  /**< 单个bounce buffer大小（字节） */
    uint32_t bounce_pos;               /**< 当前在帧缓冲中的位置（字节） */

    /* 双缓冲管理 */
    volatile uint8_t display_idx;      /**< 当前DMA正在显示的帧缓冲索引 (0 or 1) */
    volatile uint8_t write_idx;        /**< 当前应用层写入的帧缓冲索引 (0 or 1) */
    volatile uint8_t swap_pending;     /**< 是否有新帧待切换 (0 or 1) */

    /* 传输状态 */
    bool is_running;                   /**< 是否正在运行 */
} lisa_rgb_priv_t;

/* ========================================================================
 * 辅助宏定义
 * ======================================================================== */

#define RGB_LOCK(priv)                                      \
    do {                                                    \
        if ((priv)->lock) {                                 \
            lisa_mutex_lock((priv)->lock, LISA_OS_WAIT_FOREVER); \
        }                                                   \
    } while (0)

#define RGB_UNLOCK(priv)                                    \
    do {                                                    \
        if ((priv)->lock) {                                 \
            lisa_mutex_unlock((priv)->lock);                \
        }                                                   \
    } while (0)

/* ========================================================================
 * HAL 层映射函数
 * ======================================================================== */

static inline int hal_status_to_err(int status)
{
    return (status == CSK_DRIVER_OK) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_IO;
}

static rgb_emFormatIn to_hal_input_format(lisa_rgb_input_format_t fmt)
{
    switch (fmt) {
    case LISA_RGB_INPUT_FORMAT_RGB888:
        return RGB_INPUT_FORMAT_RGB888;
    case LISA_RGB_INPUT_FORMAT_XRGB8888:
        return RGB_INPUT_FORMAT_XRGB8888;
    case LISA_RGB_INPUT_FORMAT_RGB565:
    default:
        return RGB_INPUT_FORMAT_RGB565;
    }
}

static rgb_emFormatOut to_hal_output_format(lisa_rgb_output_format_t fmt)
{
    switch (fmt) {
    case LISA_RGB_OUTPUT_FORMAT_RGB888:
        return RGB_OUTPUT_FORMAT_RGB888;
    case LISA_RGB_OUTPUT_FORMAT_RGB666:
        return RGB_OUTPUT_FORMAT_RGB666;
    case LISA_RGB_OUTPUT_FORMAT_BGR888:
        return RGB_OUTPUT_FORMAT_BGR888;
    case LISA_RGB_OUTPUT_FORMAT_BGR666:
        return RGB_OUTPUT_FORMAT_BGR666;
    case LISA_RGB_OUTPUT_FORMAT_BGR565:
        return RGB_OUTPUT_FORMAT_BGR565;
    case LISA_RGB_OUTPUT_FORMAT_RGB565:
    default:
        return RGB_OUTPUT_FORMAT_RGB565;
    }
}

static rgb_emPol to_hal_polarity(lisa_rgb_polarity_t pol)
{
    return (pol == LISA_RGB_POLARITY_POSITIVE) ? RGB_POLARITY_POSITIVE : RGB_POLARITY_NEGATIVE;
}

/* ========================================================================
 * Bounce Buffer 管理
 * ======================================================================== */

/**
 * @brief 填充bounce buffer
 * 从当前显示的帧缓冲拷贝数据到指定的bounce buffer
 * @param priv 私有数据指针
 * @param bb_idx bounce buffer索引 (0 或 1)
 * @return 是否完成一帧传输 (1=完成, 0=未完成)
 */
static int bounce_buffer_fill(lisa_rgb_priv_t *priv, uint8_t bb_idx)
{
    // 从当前显示的帧缓冲读取数据
    uint8_t *src = priv->fb_buf[priv->display_idx];
    uint8_t *dst = priv->bounce_buffer[bb_idx];
    uint32_t copy_size = priv->bb_size;

    // 检查是否到帧末尾，调整拷贝大小
    if (priv->bounce_pos + copy_size > priv->fb_size) {
        copy_size = priv->fb_size - priv->bounce_pos;
    }

    // 从帧缓冲拷贝到bounce buffer
    memcpy(dst, src + priv->bounce_pos, copy_size);

#if CONFIG_DCACHE_ENABLE
    // 刷新cache确保DMA能读到最新数据
    extern void HAL_FlushDCache_by_Addr(uint32_t *addr, uint32_t len);
    HAL_FlushDCache_by_Addr((uint32_t*)dst, copy_size);
#endif

    priv->bounce_pos += copy_size;

    // 检查是否完成一帧
    if (priv->bounce_pos >= priv->fb_size) {
        priv->bounce_pos = 0;
        return 1;  // 一帧完成
    }
    return 0;
}

/* ========================================================================
 * RGB 事件回调
 * ======================================================================== */

static void rgb_event_handler(uint32_t event, void *workspace)
{
    (void)workspace;

    switch (event) {
    case RGB_IRQ_EVENT_SOF:
        // 帧开始事件
        break;
    case RGB_IRQ_EVENT_EOF:
        // 帧结束事件
        LISA_LOGD(LOG_TAG, "RGB_IRQ_EVENT_EOF");
        break;
    case RGB_IRQ_EVENT_FIFO_RD_EMPTY:
        // FIFO 读空事件 - 可能需要补充数据
        LISA_LOGW(LOG_TAG, "RGB_IRQ_EVENT_FIFO_RD_EMPTY");
        break;
    case RGB_IRQ_EVENT_FIFO_RD_FULL:
        // FIFO 读满事件
        LISA_LOGW(LOG_TAG, "RGB_IRQ_EVENT_FIFO_RD_FULL");
        break;
    default:
        break;
    }
}

/* ========================================================================
 * DMA 事件回调
 * ======================================================================== */

static void gpdma_event_callback(uint32_t event, void *workspace)
{
    lisa_rgb_priv_t *priv = (lisa_rgb_priv_t *)workspace;
    if (!priv) {
        return;
    }

    int frame_done = 0;
    uint8_t completed_bb = 0;

    // 确定刚完成传输的bounce buffer索引
    if (event & CSK_GPDMA_EVENT_PIPO0_DONE) {
        completed_bb = 0;
    } else if (event & CSK_GPDMA_EVENT_PIPO1_DONE) {
        completed_bb = 1;
    } else {
        // 其他事件处理
        LOGW("gpdma_event_callback event:%x", event);
        return;
    }

    // 填充刚完成的bounce buffer（为下一次传输准备）
    frame_done = bounce_buffer_fill(priv, completed_bb);

    // 一帧传输完成，检查是否需要切换到新的帧缓冲
    if (frame_done) {
        if (priv->swap_pending) {
            // 有新帧待显示：切换显示缓冲区
            priv->display_idx = priv->write_idx;
            priv->swap_pending = 0;

            // 释放写入信号量，允许应用层写入新帧
            if (priv->write_sem) {
                lisa_semaphore_give(priv->write_sem);
            }
            // bounce_pos 已在 bounce_buffer_fill 中重置为 0
        }
        // 如果没有新帧，继续显示当前帧（自动重复）
    }

    // 重新加载DMA传输（即使地址相同也需要reload）
    GPDMA_PiPo_Reload(priv->gpdma_ch, priv->bounce_buffer[completed_bb], NULL);
}

/* ========================================================================
 * RGB API 实现
 * ======================================================================== */

static int arcs_rgb_start(lisa_device_t *dev)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_rgb_priv_t *priv = (lisa_rgb_priv_t *)dev->priv_data;

    RGB_LOCK(priv);

    if (priv->is_running) {
        LISA_LOGW(LOG_TAG, "RGB already running, stop first");
        RGB_UNLOCK(priv);
        return LISA_DEVICE_ERR_BUSY;
    }

    // 预填充两个 bounce buffer
    bounce_buffer_fill(priv, 0);
    bounce_buffer_fill(priv, 1);

    // 启动 Ping-Pong DMA（使用 bounce buffer）
    int ret = GPDMA_Start_PiPo(priv->gpdma_ch,
                               priv->bounce_buffer[0],
                               priv->bounce_buffer[1],
                               (void *)RGB0_Buf(),
                               (void *)RGB0_Buf(),
                               priv->bb_size / 4);  // 以 WORD 为单位
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "GPDMA_Start_PiPo failed (%d)", ret);
        RGB_UNLOCK(priv);
        return LISA_DEVICE_ERR_IO;
    }
    LISA_LOGI(LOG_TAG, "GPDMA Ping-Pong started (bounce mode, bb_size: %lu words)",
              (unsigned long)(priv->bb_size / 4));

    // 启动 RGB 传输
    ret = RGB_Start(priv->hal_rgb);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "RGB_Start failed (%d)", ret);
        RGB_UNLOCK(priv);
        return LISA_DEVICE_ERR_IO;
    }

    priv->is_running = true;
    LISA_LOGI(LOG_TAG, "RGB transmission started");

    RGB_UNLOCK(priv);
    return LISA_DEVICE_OK;
}

static int arcs_rgb_stop(lisa_device_t *dev)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_rgb_priv_t *priv = (lisa_rgb_priv_t *)dev->priv_data;

    RGB_LOCK(priv);

    if (!priv->is_running) {
        RGB_UNLOCK(priv);
        return LISA_DEVICE_OK;
    }

    // 停止 RGB 传输
    int ret = RGB_Stop(priv->hal_rgb);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "RGB_Stop failed (%d)", ret);
        RGB_UNLOCK(priv);
        return LISA_DEVICE_ERR_IO;
    }

    priv->is_running = false;

    RGB_UNLOCK(priv);
    return LISA_DEVICE_OK;
}

static int arcs_rgb_wait_done(lisa_device_t *dev, uint32_t timeout_ms)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_rgb_priv_t *priv = (lisa_rgb_priv_t *)dev->priv_data;

    if (!priv->done_sem) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    if (lisa_semaphore_take(priv->done_sem, timeout_ms) != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "wait_done timeout");
        return LISA_DEVICE_ERR_TIMEOUT;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 更新图像数据（Bounce Buffer 模式专用）
 * @param dev 设备实例
 * @param buf 图像数据缓冲区
 * @param size 数据大小（字节）
 * @return 0=成功，负数=错误码
 *
 * @note 此函数用于 Bounce Buffer 模式，将新图像数据拷贝到写缓冲区
 * @note 函数会等待上一帧显示完成后才允许写入
 */
static int arcs_rgb_update_framebuffer(lisa_device_t *dev, const void *buf, uint32_t size)
{
    if (!dev || !dev->priv_data || !buf || size == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_rgb_priv_t *priv = (lisa_rgb_priv_t *)dev->priv_data;

    if (size > priv->fb_size) {
        LISA_LOGW(LOG_TAG, "input size %lu exceeds fb size %lu", (unsigned long)size, (unsigned long)priv->fb_size);
        size = priv->fb_size;
    }

    // 1. 等待写入信号量（确保不与上一次写入冲突）
    if (!priv->write_sem || lisa_semaphore_take(priv->write_sem, 1000) != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "write_sem timeout");
        return LISA_DEVICE_ERR_TIMEOUT;
    }

    // 2. 选择写缓冲区（非显示缓冲区）
    uint8_t write_idx = 1 - priv->display_idx;

    // 3. 拷贝数据到写缓冲区（LVGL buf -> framebuffer）
    memcpy(priv->fb_buf[write_idx], buf, size);

    // 4. 标记待切换（中断中会检测并切换）
    priv->write_idx = write_idx;
    priv->swap_pending = 1;

    return LISA_DEVICE_OK;
}

/* ========================================================================
 * 设备初始化
 * ======================================================================== */

static int arcs_rgb_setup(lisa_device_t *dev, const lisa_display_bus_rgb_config_t *config)
{
    if (!dev || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_rgb_priv_t *priv = (lisa_rgb_priv_t *)dev->priv_data;

    // 配置 RGB 初始化参数
    RGB_InitTypeDef rgb_init = {0};
    rgb_init.frms = RGB_FRAME_CONTINUE;
    rgb_init.wires = RGB_OUTPUT_WIRES_24;
    rgb_init.sync = RGB_SYNC_MODE_SYNC_DE;
    rgb_init.de_continue = true;
    rgb_init.format_in = to_hal_input_format(config->input_format);
    rgb_init.format_out = to_hal_output_format(config->output_format);
    rgb_init.out_lsb = config->output_lsb_first;

    // 时序参数
    rgb_init.img_width = config->timings.h_res;
    rgb_init.img_height = config->timings.v_res;
    rgb_init.h_pulse_width = config->timings.h_pulse_width;
    rgb_init.v_pulse_width = config->timings.v_pulse_width;
    rgb_init.h_front_blanking = config->timings.h_front_blanking;
    rgb_init.h_back_blanking = config->timings.h_back_blanking;
    rgb_init.v_front_blanking = config->timings.v_front_blanking;
    rgb_init.v_back_blanking = config->timings.v_back_blanking;

    // 信号极性
    rgb_init.VSPolarity = to_hal_polarity(config->vsync_polarity);
    rgb_init.HSPolarity = to_hal_polarity(config->hsync_polarity);
    rgb_init.DEPolarity = to_hal_polarity(config->de_polarity);
    rgb_init.CLKPolarity = to_hal_polarity(config->pclk_polarity);

    // 时钟频率
    rgb_init.clk_hz = config->pclk_hz;

    // 初始化 RGB 外设
    int ret = RGB_Initialize(priv->hal_rgb, rgb_event_handler, &rgb_init);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "RGB_Initialize failed (%d)", ret);
        lisa_semaphore_delete(priv->done_sem);
        lisa_mutex_delete(priv->lock);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    // 启用时钟输出
    ret = RGB_EnableClockout(config->pclk_hz);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "RGB_EnableClockout failed (%d)", ret);
        RGB_Uninitialize(priv->hal_rgb);
        lisa_semaphore_delete(priv->done_sem);
        lisa_mutex_delete(priv->lock);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    // 初始化 GPDMA（用于数据传输）
    ret = GPDMA_Initialize();
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "GPDMA_Initialize failed (%d)", ret);
        RGB_DisableClockout();
        RGB_Uninitialize(priv->hal_rgb);
        lisa_semaphore_delete(priv->done_sem);
        lisa_mutex_delete(priv->lock);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    // 配置 GPDMA
    csk_gpdma_init_t gpdma_cfg = {
        .dma_ch = priv->gpdma_ch,
        .burst_len = gpdma_burst_len_8spl,
        .sample_unit = gpdma_sample_unit_word,
        .src_mode = address_mode_pipo,
        .dst_mode = address_mode_pipo,
        .tfr_mode = tfr_mode_m2p,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .prio_lvl = prio_mode_vhigh,
        .handshake = qspi_hs_num0,
    };

    ret = GPDMA_Config(&gpdma_cfg, gpdma_event_callback, priv);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "GPDMA_Config failed (%d)", ret);
        RGB_DisableClockout();
        RGB_Uninitialize(priv->hal_rgb);
        lisa_semaphore_delete(priv->write_sem);
        lisa_semaphore_delete(priv->done_sem);
        lisa_mutex_delete(priv->lock);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    // 计算帧缓冲区大小
    uint32_t bpp = (config->input_format == LISA_RGB_INPUT_FORMAT_RGB565) ? 16 : 24;
    uint32_t w = config->timings.h_res;
    uint32_t h = config->timings.v_res;
    priv->fb_size = (w * h * bpp) / 8;

    // ========== Bounce Buffer 模式初始化 ==========
    priv->bb_size = config->bounce_buffer_size * bpp / 8;

    // 确保 bounce buffer 大小是帧缓冲大小的整数因子
    if (priv->fb_size % priv->bb_size != 0) {
        LISA_LOGE(LOG_TAG, "fb_size(%lu) must be multiple of bb_size(%lu)",
                      (unsigned long)priv->fb_size, (unsigned long)priv->bb_size);
        RGB_DisableClockout();
        RGB_Uninitialize(priv->hal_rgb);
        lisa_semaphore_delete(priv->write_sem);
        lisa_semaphore_delete(priv->done_sem);
        lisa_mutex_delete(priv->lock);
        return LISA_DEVICE_ERR_INVALID;
    }

    LISA_LOGI(LOG_TAG, "Bounce buffer mode enabled: bb_size=%lu, transfers_per_frame=%lu",
                (unsigned long)priv->bb_size, (unsigned long)(priv->fb_size / priv->bb_size));

    // 分配帧缓冲（PSRAM）- 双缓冲机制
    for (int i = 0; (i < 2) && (priv->bb_size > 0); i++) {
        priv->fb_buf[i] = (uint8_t *)lisa_mem_align_alloc(32, priv->fb_size);
        if (!priv->fb_buf[i]) {
            LISA_LOGE(LOG_TAG, "alloc fb %d failed (%lu bytes)", i, (unsigned long)priv->fb_size);
            // 清理
            for (int j = 0; j < i; j++) {
                lisa_mem_free(priv->fb_buf[j]);
            }
            RGB_DisableClockout();
            RGB_Uninitialize(priv->hal_rgb);
            lisa_semaphore_delete(priv->write_sem);
            lisa_semaphore_delete(priv->done_sem);
            lisa_mutex_delete(priv->lock);
            return LISA_DEVICE_ERR_NO_MEM;
        }
        // 初始化为白色测试图案
        memset(priv->fb_buf[i], 0xFF, priv->fb_size);
        LISA_LOGI(LOG_TAG, "alloc fb%d addr: %p (PSRAM)", i, priv->fb_buf[i]);
    }

    // 初始化双缓冲索引
    priv->display_idx = 0;   // 初始显示 fb_buf[0]
    priv->write_idx = 0;     // 尚未写入
    priv->swap_pending = 0;  // 无待切换帧

    // 分配 bounce buffer（内部RAM）
    for (int i = 0; i < 2; i++) {
        priv->bounce_buffer[i] = (uint8_t *)inram_malloc(32, priv->bb_size);
        if (!priv->bounce_buffer[i]) {
            LISA_LOGE(LOG_TAG, "alloc bounce buffer %d failed (%lu bytes)", i, (unsigned long)priv->bb_size);
            // 清理
            for (int j = 0; j < i; j++) {
                inram_free(priv->bounce_buffer[j]);
            }
            for (int j = 0; j < 2; j++) {
                lisa_mem_free(priv->fb_buf[j]);
            }
            RGB_DisableClockout();
            RGB_Uninitialize(priv->hal_rgb);
            lisa_semaphore_delete(priv->write_sem);
            lisa_semaphore_delete(priv->done_sem);
            lisa_mutex_delete(priv->lock);
            return LISA_DEVICE_ERR_NO_MEM;
        }
        memset(priv->bounce_buffer[i], 0x00, priv->bb_size);
        LISA_LOGI(LOG_TAG, "alloc bounce_buffer[%d] addr: %p (Internal RAM)", i, priv->bounce_buffer[i]);
    }
    priv->bounce_pos = 0;

    LISA_LOGI(LOG_TAG, "Bounce buffer initialized (fb_size: %lu, bb_size: %lu)",
                (unsigned long)priv->fb_size, (unsigned long)priv->bb_size);

    LISA_LOGI(LOG_TAG, "RGB device initialized (resolution: %dx%d, pclk: %lu Hz)",
              config->timings.h_res, config->timings.v_res, (unsigned long)config->pclk_hz);

    return LISA_DEVICE_OK;
}

/* ========================================================================
 * 设备 API 和注册
 * ======================================================================== */

static const lisa_rgb_api_t arcs_rgb_api = {
    .stop               = arcs_rgb_stop,
    .setup              = arcs_rgb_setup,
    .start              = arcs_rgb_start,
    .wait_done          = arcs_rgb_wait_done,
    .update_framebuffer = arcs_rgb_update_framebuffer,
};

static lisa_rgb_priv_t rgb_priv;

/**
 * @brief RGB 设备初始化入口函数
 */
int lisa_rgb0_init(void)
{
    memset(&rgb_priv, 0, sizeof(rgb_priv));

    lisa_rgb_pinmux();

    rgb_priv.hal_rgb = RGB0();
    rgb_priv.gpdma_ch = CONFIG_LISA_RGB_GPDMA_CH;

    // 创建互斥锁
    rgb_priv.lock = lisa_mutex_create();
    if (!rgb_priv.lock) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_NO_MEM;
    }

    // 创建信号量
    rgb_priv.done_sem = lisa_semaphore_create(1);
    if (!rgb_priv.done_sem) {
        LISA_LOGE(LOG_TAG, "Failed to create semaphore");
        lisa_mutex_delete(rgb_priv.lock);
        return LISA_DEVICE_ERR_NO_MEM;
    }

    // 创建写入信号量（用于 Bounce Buffer 模式）
    rgb_priv.write_sem = lisa_semaphore_create(1);
    if (!rgb_priv.write_sem) {
        LISA_LOGE(LOG_TAG, "Failed to create write semaphore");
        lisa_semaphore_delete(rgb_priv.done_sem);
        lisa_mutex_delete(rgb_priv.lock);
        return LISA_DEVICE_ERR_NO_MEM;
    }

    lisa_semaphore_give(rgb_priv.write_sem);  // 初始化为可用状态
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(rgb0, &arcs_rgb_api, &rgb_priv, NULL, &lisa_rgb0_init, LISA_DEVICE_PRIORITY_NORMAL);
