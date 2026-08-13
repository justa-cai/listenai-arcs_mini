/**
 * @file lisa_dvp_venusa.c
 * @brief Venusa 平台 LISA DVP 设备驱动实现。
 *
 * 本文件在不改变公共 lisa_dvp API 的前提下，将 LISA DVP 抽象适配到
 * Venusa HAL DVP + DMA 通路。默认使用 CMNDMA（对应 ARCS CPDMA 语义）；
 * 关闭 CONFIG_LISA_DVP_VENUSA_USE_CMNDMA 后使用 DMA2D（对应 ARCS GPDMA
 * 语义）。CMNDMA 后端通过 dma_channel_start_pipo() 实现硬件 Ping-Pong。
 */

#include <stdint.h>
#include <string.h>

#include "cache.h"
#include "lisa_mutex.h"
#include "lisa_device.h"
#include "lisa_dvp.h"
#include "Driver_DVP.h"
#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
#include "dma.h"
#else
#include "Driver_DMA2D.h"
#endif
#include "board.h"
#include "venusa_ap.h"

#define LOG_TAG "lisa_dvp"
#include "lisa_log.h"

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

#define VENUSA_DVP_DMA_ALIGN 4U
#if CONFIG_DCACHE_ENABLE
#define VENUSA_DVP_BUFFER_ALIGN HAL_DCACHE_CFG_LINE_SIZE
#else
#define VENUSA_DVP_BUFFER_ALIGN VENUSA_DVP_DMA_ALIGN
#endif

/**
 * @brief Venusa DVP 采集模式。
 */
typedef enum {
    LISA_DVP_VENUSA_MODE_NORMAL = 0, /**< 单缓冲普通采集模式。 */
    LISA_DVP_VENUSA_MODE_PINGPONG,   /**< 双缓冲 Ping-Pong 采集模式。 */
} lisa_dvp_venusa_mode_t;

/**
 * @brief Venusa DVP 驱动私有上下文。
 */
typedef struct {
    void *hal_handler;            /**< DVP HAL 句柄。 */
    lisa_dvp_callback_t callback; /**< 应用注册的帧完成回调。 */
    void *user_data;              /**< 透传给应用回调的用户数据。 */
    bool initialized;             /**< 驱动是否已完成 setup。 */
    bool clockout_enabled;        /**< 是否已打开 DVP MCLK 输出。 */
    uint8_t dma_ch;               /**< Venusa DMA 通道号：CMNDMA(即CPDMA) 或 DMA2D(即GPDMA)。 */
#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    bool cmndma_reserved;         /**< 是否已独占保留 CMNDMA 通道。 */
#endif
    volatile bool stop_flag;      /**< 停止标记，true 表示未采集或已停止。 */
    lisa_mutex_t *lock;           /**< 保护 setup/start/stop 等控制路径的互斥锁。 */
    void *volatile active_buf;    /**< 当前 DMA 正在写入的目标缓冲区。 */
    volatile uint32_t active_len; /**< 当前 DMA 传输长度，单位字节。 */

    lisa_dvp_venusa_mode_t mode;    /**< 当前采集模式。 */
    void *ping_buf;                 /**< Ping 缓冲区地址。 */
    void *pong_buf;                 /**< Pong 缓冲区地址。 */
    uint32_t pingpong_len;          /**< 单个 Ping/Pong 缓冲区长度，单位字节。 */
    volatile int8_t active_slot;    /**< 当前/最近 DMA 使用的缓冲区编号，0=Ping，1=Pong。 */
    volatile int8_t completed_slot; /**< 最近完成的缓冲区编号，供 reload_pingpong 更新。 */
} lisa_dvp_priv_t;

static lisa_dvp_priv_t dvp_priv;

/**
 * @brief 将 LISA DVP 极性枚举转换为 Venusa HAL 极性枚举。
 *
 * @param polarity LISA DVP 极性枚举。
 * @return Venusa HAL DVP 极性枚举。
 */
static DVP_emPol venusa_dvp_convert_polarity(lisa_dvp_polarity_t polarity)
{
    return (polarity == LISA_DVP_POL_RISING) ? DVP_POL_RISING : DVP_POL_FALLING;
}

/**
 * @brief 将 LISA DVP 数据对齐方式转换为 Venusa HAL 数据对齐方式。
 *
 * @param align LISA DVP 数据对齐方式。
 * @return Venusa HAL DVP 数据对齐方式。
 */
static DVP_emDataAlign venusa_dvp_convert_data_align(lisa_dvp_data_align_t align)
{
    return (align == LISA_DVP_DATA_ALIGN_LEFT) ? DVP_DATA_ALIGN_LEFT : DVP_DATA_ALIGN_RIGHT;
}

/**
 * @brief 将 LISA DVP 输入格式转换为 Venusa HAL 输入格式。
 *
 * @param input_format LISA DVP 输入格式。
 * @param hal_format 输出的 Venusa HAL DVP 输入格式。
 * @return LISA_DEVICE_OK 转换成功，否则返回 LISA 设备错误码。
 */
static int venusa_dvp_convert_input_format(lisa_dvp_input_format_t input_format, DVP_emInputFormat *hal_format)
{
    if (!hal_format) {
        return LISA_DEVICE_ERR_INVALID;
    }

    switch (input_format) {
    case LISA_DVP_INPUT_FORM_YUV422_Y0CBY1CR:
        *hal_format = DVP_INPUT_FORM_YUV422_Y0CBY1CR;
        return LISA_DEVICE_OK;
    case LISA_DVP_INPUT_FORM_LUMINA_8BIT:
        *hal_format = DVP_INPUT_FORM_LUMINA_8BIT;
        return LISA_DEVICE_OK;
    default:
        return LISA_DEVICE_ERR_INVALID;
    }
}

/**
 * @brief 检查 DMA 通道号是否合法。
 *
 * @param dma_ch 待检查的 DMA 通道号。
 * @return true 通道号合法，false 通道号越界。
 */
static bool venusa_dvp_dma_ch_valid(uint8_t dma_ch)
{
#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    return dma_ch < DMA_NUMBER_OF_CHANNELS;
#else
    return dma_ch < CSK_DMA2D_MAX_CHANNEL_NUM;
#endif
}

/**
 * @brief 检查 DMA 目标缓冲区是否满足 DMA 和 cache 传输要求。
 *
 * @param buf 目标缓冲区地址。
 * @param len 传输长度，单位字节。
 * @return true 缓冲区合法，false 缓冲区为空、长度为 0 或未按要求对齐。
 */
static bool venusa_dvp_buffer_valid(const void *buf, uint32_t len)
{
    return buf && len && ((((uintptr_t)buf) % VENUSA_DVP_BUFFER_ALIGN) == 0U) &&
           ((len % VENUSA_DVP_BUFFER_ALIGN) == 0U) && ((((uintptr_t)buf) % VENUSA_DVP_DMA_ALIGN) == 0U) &&
           ((len % VENUSA_DVP_DMA_ALIGN) == 0U);
}

/**
 * @brief 使 DMA 目标缓冲区对应的 DCache 失效。
 *
 * @param buf 目标缓冲区地址。
 * @param len 缓冲区长度，单位字节。
 */
static void venusa_dvp_cache_invalidate(void *buf, uint32_t len)
{
#if CONFIG_DCACHE_ENABLE
    if (buf && len) {
        HAL_InvalidateDCache_by_Addr((uint32_t *)(uintptr_t)buf, len);
    }
#else
    (void)buf;
    (void)len;
#endif
}

/**
 * @brief 控制路径加锁；中断上下文下跳过互斥量。
 *
 * 用户帧回调在 DMA 中断里被同步调用，可能反过来调用 stop/reload 等控制
 * 接口。FreeRTOS 互斥量不允许在 ISR 中 take/give，因此中断上下文下不取锁，
 * 依赖 volatile stop_flag 与幂等的 HAL stop 操作保证安全。
 *
 * @return true 已加锁（任务上下文），false 处于中断上下文未加锁。
 */
static bool venusa_dvp_lock(void)
{
    if (xPortIsInsideInterrupt() == pdTRUE) {
        return false;
    }
    lisa_mutex_lock(dvp_priv.lock, -1);
    return true;
}

/**
 * @brief 与 venusa_dvp_lock() 配对的解锁；仅在确实加锁时释放。
 *
 * @param locked venusa_dvp_lock() 的返回值。
 */
static void venusa_dvp_unlock(bool locked)
{
    if (locked) {
        lisa_mutex_unlock(dvp_priv.lock);
    }
}

/**
 * @brief 进入屏蔽全局中断的临界区，返回原中断使能状态。
 *
 * 控制路径（如 reload_pingpong）与 DMA 完成中断共享 completed_slot /
 * ping_buf / pong_buf，而 FreeRTOS 互斥量无法排斥 ISR，因此
 * 需要短暂关全局中断来保证读-清-写的原子性。中断上下文下全局中断本已关闭，
 * 此处为可嵌套的空操作。
 *
 * @return 进入临界区前全局中断是否使能，供 venusa_dvp_isr_restore() 还原。
 */
static inline uint8_t venusa_dvp_isr_disable(void)
{
    uint8_t enabled = GINT_enabled();
    if (enabled) {
        disable_GINT();
    }
    return enabled;
}

/**
 * @brief 还原 venusa_dvp_isr_disable() 保存的全局中断状态。
 *
 * @param enabled venusa_dvp_isr_disable() 的返回值。
 */
static inline void venusa_dvp_isr_restore(uint8_t enabled)
{
    if (enabled) {
        enable_GINT();
    }
}

/**
 * @brief 配置 DVP 到 DMA 的硬件握手选择。
 *
 * CMNDMA(即CPDMA) 后端配置为 1/1/1 路由；
 * DMA2D(即GPDMA) 后端配置为 0/1/0 路由。
 */
static void venusa_dvp_config_dma_handshake(void)
{
#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_03 = 1;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_15 = 1;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_19 = 1;
#else
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_03 = 0;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_15 = 1;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_19 = 0;
#endif
}

#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
/**
 * @brief 获取 DVP CMNDMA(即CPDMA) 控制寄存器配置。
 *
 * DVP FIFO 作为外设源，源地址固定，目标内存递增；宽度固定为 word，
 * 与 TEST_DVP_DMA_SEL demo 保持一致。
 */
static uint32_t venusa_dvp_dma_control(void)
{
    return DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_WORD) |
           DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_WORD) | DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_8) |
           DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_8) | DMA_CH_CTLL_TTFC_P2M | DMA_CH_CTLL_DMS(0) |
           DMA_CH_CTLL_SMS(1) | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_DST_INC;
}

/**
 * @brief 获取 DVP CMNDMA(即CPDMA) 低 32 位配置寄存器。
 */
static uint32_t venusa_dvp_dma_config_low(void)
{
    return DMA_CH_CFGL_CH_PRIOR(0);
}

/**
 * @brief 获取 DVP CMNDMA(即CPDMA) 高 32 位配置寄存器。
 */
static uint32_t venusa_dvp_dma_config_high(void)
{
    return DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_SRC_PER(3);
}

/**
 * @brief 将字节长度转换为 word 传输项数。
 *
 * @note 调用方必须保证 len 已通过 venusa_dvp_buffer_valid()（4 字节对齐），
 *       否则会向下取整丢弃尾部不足一个 word 的字节。
 */
static uint32_t venusa_dvp_dma_words(uint32_t len)
{
    return len / sizeof(uint32_t);
}
#endif

/**
 * @brief 启动一次 DVP FIFO 到应用缓冲区的 DMA 传输。
 *
 * @param buf 目标帧缓冲区地址。
 * @param len 传输长度，单位字节。
 * @return LISA_DEVICE_OK 启动成功，否则返回 LISA 设备错误码。
 *
 * @note start/reload 控制路径持锁调用，普通模式每次装载一个单块传输。
 */
static int venusa_dvp_dma_start_locked(void *buf, uint32_t len)
{
    venusa_dvp_cache_invalidate(buf, len);

    dvp_priv.active_buf = buf;
    dvp_priv.active_len = len;

#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    int32_t ret = dma_channel_configure(dvp_priv.dma_ch, DVP0_Buf(), (uint32_t)(uintptr_t)buf,
                                        venusa_dvp_dma_words(len), venusa_dvp_dma_control(),
                                        venusa_dvp_dma_config_low(), venusa_dvp_dma_config_high(), 0, 0);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "dma_channel_configure failed: %d", ret);
        dvp_priv.active_buf = NULL;
        dvp_priv.active_len = 0;
        return LISA_DEVICE_ERR_IO;
    }
#else
    int32_t ret = DMA2D_Start_Normal((csk_dma2d_ch_t)dvp_priv.dma_ch, (void *)DVP0_Buf(), buf, len);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "DMA2D_Start_Normal failed: %d", ret);
        dvp_priv.active_buf = NULL;
        dvp_priv.active_len = 0;
        return LISA_DEVICE_ERR_IO;
    }
#endif
    return LISA_DEVICE_OK;
}

/**
 * @brief 停止当前 DMA 传输。
 *
 * @param wait_done 是否等待 DMA 通道真正关闭；ISR 中必须传 false。
 */
static void venusa_dvp_dma_stop(bool wait_done)
{
    if (wait_done && xPortIsInsideInterrupt() == pdTRUE) {
        wait_done = false;
    }

#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    if (dvp_priv.mode == LISA_DVP_VENUSA_MODE_PINGPONG) {
        (void)dma_channel_cancel_pipo(dvp_priv.dma_ch);
    }
    (void)dma_channel_disable(dvp_priv.dma_ch, wait_done ? 1 : 0);
#else
    (void)wait_done;
    (void)DMA2D_Stop((csk_dma2d_ch_t)dvp_priv.dma_ch);
#endif
}

/**
 * @brief 释放已初始化的 DVP/DMA 资源并复位私有状态。
 *
 * @note 调用者必须已经持有 dvp_priv.lock。
 */
static void venusa_dvp_release_locked(void)
{
    if (!dvp_priv.initialized) {
        return;
    }

    if (!dvp_priv.stop_flag) {
        DVP_Stop(dvp_priv.hal_handler);
        venusa_dvp_dma_stop(true);
    }

    if (dvp_priv.clockout_enabled) {
        DVP_DisableClockout();
    }

    DVP_Uninitialize(dvp_priv.hal_handler);
#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    if (dvp_priv.cmndma_reserved) {
        dma_channel_unreserve(dvp_priv.dma_ch);
    }
#endif

    dvp_priv.hal_handler = NULL;
    dvp_priv.callback = NULL;
    dvp_priv.user_data = NULL;
    dvp_priv.initialized = false;
    dvp_priv.clockout_enabled = false;
    dvp_priv.dma_ch = 0;
#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    dvp_priv.cmndma_reserved = false;
#endif
    dvp_priv.stop_flag = true;
    dvp_priv.active_buf = NULL;
    dvp_priv.active_len = 0;
    dvp_priv.mode = LISA_DVP_VENUSA_MODE_NORMAL;
    dvp_priv.ping_buf = NULL;
    dvp_priv.pong_buf = NULL;
    dvp_priv.pingpong_len = 0;
    dvp_priv.active_slot = -1;
    dvp_priv.completed_slot = -1;
}

/**
 * @brief Venusa DVP HAL 中断事件回调。
 *
 * @param event DVP HAL 上报的中断事件。
 * @param param HAL 事件参数，当前未使用。
 */
static void venusa_dvp_irq_callback(DVP_emIrqEvent event, uint32_t param)
{
    (void)param;

    if (dvp_priv.stop_flag) {
        return;
    }

    switch (event) {
    case DVP_IRQ_EVENT_FIFO_UNFLOW:
    case DVP_IRQ_EVENT_FIFO_OVFLOW:
    case DVP_IRQ_EVENT_EOF_CNT_ABNOR:
    case DVP_IRQ_EVENT_H_SYNC_ABNOR:
    case DVP_IRQ_EVENT_PIXEL_ABNOR:
        if(DVP_IRQ_EVENT_EOF_CNT_ABNOR == event) {
            LISA_LOGD(LOG_TAG, "DVP irq event: %d", event);
            // for debug
            // LISA_LOGD(LOG_TAG, "VIC_DEBUG    *0x%08x = 0x%08x", &IP_DVP_IN->REG_IMAGE_VIC_DEBUG.all,
            //           IP_DVP_IN->REG_IMAGE_VIC_DEBUG.all);
            // LISA_LOGD(LOG_TAG, "ST_DEBUG     *0x%08x = 0x%08x", &IP_DVP_IN->REG_ST_DEBUG.all,
            //           IP_DVP_IN->REG_ST_DEBUG.all);
            // LISA_LOGD(LOG_TAG, "SERSOR_DEBUG *0x%08x = 0x%08x", &IP_DVP_IN->REG_SERSOR_DEBUG.all,
            //           IP_DVP_IN->REG_SERSOR_DEBUG.all);
        }
        break;
    default:
        break;
    }
}

#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
/**
 * @brief 上报一个 DMA PiPo block 完成事件。
 *
 * @param blk DMA HAL 返回的已完成 block 信息。
 */
static void venusa_dvp_report_pipo_block(const DMA_PIPO_BLK *blk)
{
    if (!blk || !blk->dst) {
        return;
    }

    int8_t completed_slot;
    if (blk->dst == dvp_priv.ping_buf) {
        completed_slot = 0;
    } else if (blk->dst == dvp_priv.pong_buf) {
        completed_slot = 1;
    } else {
        LISA_LOGW(LOG_TAG, "Unknown DVP PiPo buffer done: %p", blk->dst);
        return;
    }

    venusa_dvp_cache_invalidate(blk->dst, dvp_priv.pingpong_len);
    dvp_priv.completed_slot = completed_slot;
    lisa_dvp_event_t dvp_event = (completed_slot == 0) ? LISA_DVP_EVENT_PING_DONE : LISA_DVP_EVENT_PONG_DONE;

    if (dvp_priv.callback) {
        dvp_priv.callback(dvp_event, dvp_priv.user_data);
    }
}

/**
 * @brief CMNDMA(即CPDMA) 传输事件回调。
 *
 * 普通模式下将 transfer complete 转换为 LISA_DVP_EVENT_DONE；Ping-Pong
 * 模式下使用 DMA HAL 的 block complete 和 pipo block 信息判断 Ping/Pong。
 *
 * @param event_info DMA 事件类型和通道号，低 8 bit 为事件类型。
 * @param xfer_bytes 已传输字节数，当前仅用于调试。
 * @param usr_param DMA 用户参数，当前未使用。
 */
static void venusa_dvp_dma_callback(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    (void)xfer_bytes;
    (void)usr_param;

    uint8_t event = event_info & 0xFF;
    if (dvp_priv.stop_flag && event != DMA_EVENT_ERROR) {
        return;
    }

    if (event == DMA_EVENT_ERROR) {
        LISA_LOGE(LOG_TAG, "DVP DMA error, event=0x%x", event_info);
        dvp_priv.stop_flag = true;
        DVP_Stop(dvp_priv.hal_handler);
        venusa_dvp_dma_stop(false);
        return;
    }

    if (dvp_priv.mode == LISA_DVP_VENUSA_MODE_NORMAL) {
        if (event != DMA_EVENT_TRANSFER_COMPLETE) {
            return;
        }

        venusa_dvp_cache_invalidate(dvp_priv.active_buf, dvp_priv.active_len);
        if (dvp_priv.callback) {
            dvp_priv.callback(LISA_DVP_EVENT_DONE, dvp_priv.user_data);
        }
        return;
    }

    if (event != DMA_EVENT_BLOCK_COMPLETE) {
        return;
    }

    DMA_PIPO_BLK done_blk = {0};
    int32_t done_cnt = dma_channel_get_pipo_blks(dvp_priv.dma_ch, &done_blk, 1);
    if (done_cnt <= 0) {
        LISA_LOGW(LOG_TAG, "No DVP PiPo block completed: %d", done_cnt);
        return;
    }

    venusa_dvp_report_pipo_block(&done_blk);
}
#else
/**
 * @brief DMA2D 传输完成回调。
 *
 * 普通模式下将 DMA2D 完成事件转换为 LISA_DVP_EVENT_DONE；Ping-Pong
 * 模式下根据 active_slot 交替上报 PING_DONE/PONG_DONE，并在用户回调
 * 返回后启动下一块缓冲区传输。
 *
 * @param event DMA2D HAL 事件位掩码。
 * @param workspace DMA2D 用户上下文，当前未使用。
 */
static void venusa_dvp_dma_callback(uint32_t event, void *workspace)
{
    (void)workspace;

    if (dvp_priv.stop_flag || !(event & CSK_DMA2D_EVENT_BLOCK_DONE)) {
        return;
    }

    venusa_dvp_cache_invalidate(dvp_priv.active_buf, dvp_priv.active_len);

    if (dvp_priv.mode == LISA_DVP_VENUSA_MODE_NORMAL) {
        if (dvp_priv.callback) {
            dvp_priv.callback(LISA_DVP_EVENT_DONE, dvp_priv.user_data);
        }
        return;
    }

    int8_t completed_slot = dvp_priv.active_slot;
    lisa_dvp_event_t dvp_event = (completed_slot == 0) ? LISA_DVP_EVENT_PING_DONE : LISA_DVP_EVENT_PONG_DONE;
    dvp_priv.completed_slot = completed_slot;

    if (dvp_priv.callback) {
        dvp_priv.callback(dvp_event, dvp_priv.user_data);
    }

    if (dvp_priv.stop_flag) {
        return;
    }

    dvp_priv.active_slot = (completed_slot == 0) ? 1 : 0;
    void *next_buf = (dvp_priv.active_slot == 0) ? dvp_priv.ping_buf : dvp_priv.pong_buf;
    if (!next_buf) {
        LISA_LOGE(LOG_TAG, "No DVP Ping-Pong buffer for slot %d", dvp_priv.active_slot);
        dvp_priv.stop_flag = true;
        DVP_Stop(dvp_priv.hal_handler);
        venusa_dvp_dma_stop(false);
        return;
    }

    int ret = venusa_dvp_dma_start_locked(next_buf, dvp_priv.pingpong_len);
    if (ret != LISA_DEVICE_OK) {
        dvp_priv.stop_flag = true;
        DVP_Stop(dvp_priv.hal_handler);
        venusa_dvp_dma_stop(false);
    }
}
#endif

/**
 * @brief 配置并初始化 Venusa DVP 设备。
 *
 * @param dev LISA 设备对象，当前驱动仅支持全局 DVP0 实例。
 * @param config LISA DVP 配置，dma_ch 按后端表示 CMNDMA(即CPDMA) 或 DMA2D(即GPDMA) 通道。
 * @param callback 应用帧完成回调。
 * @param user_data 透传给应用回调的用户数据。
 * @return LISA_DEVICE_OK 初始化成功，否则返回 LISA 设备错误码。
 */
static int venusa_dvp_setup(const lisa_device_t *dev, const lisa_dvp_config_t *config, lisa_dvp_callback_t callback,
                            void *user_data)
{
    (void)dev;

    if (!config || !venusa_dvp_dma_ch_valid(config->gpdma_ch)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    bool locked = venusa_dvp_lock();

    if (dvp_priv.initialized) {
        LISA_LOGW(LOG_TAG, "reconfigure existing DVP instance");
        venusa_dvp_release_locked();
    }

    DVP_emInputFormat input_format;
    int ret = venusa_dvp_convert_input_format(config->dvp_hal_config.input_format, &input_format);
    if (ret != LISA_DEVICE_OK) {
        venusa_dvp_unlock(locked);
        return ret;
    }

    lisa_dvp_pinmux();

    dvp_priv.hal_handler = DVP0();
    if (!dvp_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get DVP0 handler");
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    DVP_InitTypeDef hal_config = {
        .FrameWidth = config->dvp_hal_config.frame_width,
        .FrameHeight = config->dvp_hal_config.frame_height,
        .PixelOffset = config->dvp_hal_config.pixel_offset,
        .LineOffset = config->dvp_hal_config.line_offset,
        .InputFormat = input_format,
        .DataAlign = venusa_dvp_convert_data_align(config->dvp_hal_config.data_align),
        .VSPolarity = venusa_dvp_convert_polarity(config->dvp_hal_config.vsync_polarity),
        .HSPolarity = venusa_dvp_convert_polarity(config->dvp_hal_config.hsync_polarity),
        .PCKPolarity = venusa_dvp_convert_polarity(config->dvp_hal_config.pclk_polarity),
    #if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
        .BurstThreshold = 8,
    #else
        .BurstThreshold = 16,
    #endif
    };

    int32_t hal_ret = DVP_Initialize(dvp_priv.hal_handler, venusa_dvp_irq_callback, &hal_config);
    if (hal_ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "DVP_Initialize failed: %d", hal_ret);
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    hal_ret = dma_initialize();
    if (hal_ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "dma_initialize failed: %d", hal_ret);
        DVP_Uninitialize(dvp_priv.hal_handler);
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    uint8_t dma_ch = dma_channel_reserve(config->gpdma_ch, venusa_dvp_dma_callback, 0, DMA_CACHE_SYNC_NOP);
    if (dma_ch == DMA_CHANNEL_ANY) {
        LISA_LOGE(LOG_TAG, "dma_channel_reserve failed: %u", config->gpdma_ch);
        DVP_Uninitialize(dvp_priv.hal_handler);
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
#else
    hal_ret = DMA2D_Initialize();
    if (hal_ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "DMA2D_Initialize failed: %d", hal_ret);
        DVP_Uninitialize(dvp_priv.hal_handler);
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    csk_dma2d_init_t dma2d_config = {
        .dma_ch = (csk_dma2d_ch_t)config->gpdma_ch,
        .tfr_mode = tfr_mode_p2m,
        .src_basic_unit = dma2d_sample_unit_word,
        .dst_basic_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_fix,
        .dst_inc_mode = inc_mode_increase,
        .src_burst_len = dma2d_burst_len_16spl,
        .dst_burst_len = dma2d_burst_len_16spl,
        .flow_ctrl = dma2d_flow_ctrl_dma,
        .prio_lvl = prio_mode_vhigh,
        .handshake = dvp_hs_num3,
        .rd_max_len = dma2d_ahb_burst_len_default,
        .wr_max_len = dma2d_ahb_burst_len_default,
        .src_gather =
            {
                .enable = csk_func_disable,
            },
        .dst_scatter =
            {
                .enable = csk_func_disable,
            },
        .trigger =
            {
                .mode = csk_trigger_null,
                .triggered_en = csk_func_disable,
                .triggered_src_chn = dma_2d_ch0,
            },
    };

    hal_ret = DMA2D_Config(&dma2d_config, venusa_dvp_dma_callback, NULL);
    if (hal_ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "DMA2D_Config failed: %d", hal_ret);
        DVP_Uninitialize(dvp_priv.hal_handler);
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    uint8_t dma_ch = config->gpdma_ch;
#endif
    venusa_dvp_config_dma_handshake();

    dvp_priv.callback = callback;
    dvp_priv.user_data = user_data;
    dvp_priv.dma_ch = dma_ch;
#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    dvp_priv.cmndma_reserved = true;
#endif
    dvp_priv.initialized = true;
    dvp_priv.clockout_enabled = false;
    dvp_priv.stop_flag = true;
    dvp_priv.active_buf = NULL;
    dvp_priv.active_len = 0;
    dvp_priv.mode = LISA_DVP_VENUSA_MODE_NORMAL;
    dvp_priv.ping_buf = NULL;
    dvp_priv.pong_buf = NULL;
    dvp_priv.pingpong_len = 0;
    dvp_priv.active_slot = -1;
    dvp_priv.completed_slot = -1;

#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    const char *dma_name = "DMA";
#else
    const char *dma_name = "DMA2D";
#endif
    LISA_LOGI(LOG_TAG, "DVP initialized successfully with %s channel %u", dma_name, dvp_priv.dma_ch);
    venusa_dvp_unlock(locked);
    return LISA_DEVICE_OK;
}

/**
 * @brief 启动普通单缓冲采集。
 *
 * @param dev LISA 设备对象，当前未直接使用。
 * @param buf 目标帧缓冲区地址。
 * @param len 目标帧缓冲区长度，单位字节。
 * @return LISA_DEVICE_OK 启动成功，否则返回 LISA 设备错误码。
 */
static int venusa_dvp_start(const lisa_device_t *dev, void *buf, uint32_t len)
{
    (void)dev;

    if (!venusa_dvp_buffer_valid(buf, len)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    bool locked = venusa_dvp_lock();

    if (!dvp_priv.initialized) {
        LISA_LOGE(LOG_TAG, "DVP not initialized");
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (!dvp_priv.stop_flag) {
        LISA_LOGW(LOG_TAG, "DVP already started");
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_OK;
    }

    dvp_priv.mode = LISA_DVP_VENUSA_MODE_NORMAL;
    dvp_priv.stop_flag = false;

    int ret = venusa_dvp_dma_start_locked(buf, len);
    if (ret != LISA_DEVICE_OK) {
        dvp_priv.stop_flag = true;
        venusa_dvp_unlock(locked);
        return ret;
    }

    int32_t hal_ret = DVP_Start(dvp_priv.hal_handler);
    if (hal_ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "DVP_Start failed: %d", hal_ret);
        venusa_dvp_dma_stop(true);
        dvp_priv.stop_flag = true;
        dvp_priv.active_buf = NULL;
        dvp_priv.active_len = 0;
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_IO;
    }

    LISA_LOGI(LOG_TAG, "DVP capture started");
    venusa_dvp_unlock(locked);
    return LISA_DEVICE_OK;
}

/**
 * @brief 启动 Ping-Pong 双缓冲采集。
 *
 * @param dev LISA 设备对象，当前未直接使用。
 * @param ping_buf Ping 帧缓冲区地址。
 * @param pong_buf Pong 帧缓冲区地址。
 * @param len 单个缓冲区长度，单位字节。
 * @return LISA_DEVICE_OK 启动成功，否则返回 LISA 设备错误码。
 *
 * @note CMNDMA(即CPDMA) 后端使用 dma_channel_start_pipo() 硬件 LLP 环实现
 *       Ping/Pong 切换；DMA2D(即GPDMA) 后端沿用回调中软件轮转。
 */
static int venusa_dvp_start_pingpong(const lisa_device_t *dev, void *ping_buf, void *pong_buf, uint32_t len)
{
    (void)dev;

    if (!venusa_dvp_buffer_valid(ping_buf, len) || !venusa_dvp_buffer_valid(pong_buf, len)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    bool locked = venusa_dvp_lock();

    if (!dvp_priv.initialized) {
        LISA_LOGE(LOG_TAG, "DVP not initialized");
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (!dvp_priv.stop_flag) {
        LISA_LOGW(LOG_TAG, "DVP already started");
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_OK;
    }

    dvp_priv.mode = LISA_DVP_VENUSA_MODE_PINGPONG;
    dvp_priv.ping_buf = ping_buf;
    dvp_priv.pong_buf = pong_buf;
    dvp_priv.pingpong_len = len;
    dvp_priv.active_slot = 0;
    dvp_priv.completed_slot = -1;
    dvp_priv.stop_flag = false;

    int32_t hal_ret;
#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    venusa_dvp_cache_invalidate(ping_buf, len);
    venusa_dvp_cache_invalidate(pong_buf, len);

    hal_ret = dma_channel_setup(dvp_priv.dma_ch, DMA_CH_EN_XFER_INT | DMA_CH_EN_BLK_INT | DMA_CH_EN_PIPO,
                                venusa_dvp_dma_control(), venusa_dvp_dma_config_low(),
                                venusa_dvp_dma_config_high(), 0, 0);
    if (hal_ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "dma_channel_setup PiPo failed: %d", hal_ret);
        dvp_priv.stop_flag = true;
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_IO;
    }

    DMA_PIPO_BLK blocks[2] = {
        {
            .src = (void *)(uintptr_t)DVP0_Buf(),
            .dst = ping_buf,
            .size = venusa_dvp_dma_words(len),
            .flags = 0,
        },
        {
            .src = (void *)(uintptr_t)DVP0_Buf(),
            .dst = pong_buf,
            .size = venusa_dvp_dma_words(len),
            .flags = 0,
        },
    };
    uint8_t block_count = 2;
    hal_ret = dma_channel_start_pipo(dvp_priv.dma_ch, blocks, &block_count);
    if (hal_ret != CSK_DRIVER_OK || block_count < 2) {
        LISA_LOGE(LOG_TAG, "dma_channel_start_pipo failed: %d, blocks=%u", hal_ret, block_count);
        venusa_dvp_dma_stop(true);
        dvp_priv.stop_flag = true;
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_IO;
    }
#else
    int ret = venusa_dvp_dma_start_locked(ping_buf, len);
    if (ret != LISA_DEVICE_OK) {
        dvp_priv.stop_flag = true;
        venusa_dvp_unlock(locked);
        return ret;
    }
#endif

    hal_ret = DVP_Start(dvp_priv.hal_handler);
    if (hal_ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "DVP_Start failed: %d", hal_ret);
        venusa_dvp_dma_stop(true);
        dvp_priv.stop_flag = true;
        dvp_priv.active_buf = NULL;
        dvp_priv.active_len = 0;
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_IO;
    }

    LISA_LOGI(LOG_TAG, "DVP Ping-Pong capture started");
    venusa_dvp_unlock(locked);
    return LISA_DEVICE_OK;
}

/**
 * @brief 停止当前 DVP 采集。
 *
 * @param dev LISA 设备对象，当前未直接使用。
 * @return LISA_DEVICE_OK 停止成功或已经停止，否则返回 LISA 设备错误码。
 */
static int venusa_dvp_stop(const lisa_device_t *dev)
{
    (void)dev;

    bool locked = venusa_dvp_lock();

    if (!dvp_priv.initialized) {
        LISA_LOGE(LOG_TAG, "DVP not initialized");
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (dvp_priv.stop_flag) {
        LISA_LOGW(LOG_TAG, "DVP already stopped");
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_OK;
    }

    dvp_priv.stop_flag = true;
    dvp_priv.active_buf = NULL;
    dvp_priv.active_len = 0;
    DVP_Stop(dvp_priv.hal_handler);
    venusa_dvp_dma_stop(true);

    LISA_LOGI(LOG_TAG, "DVP stopped successfully");
    venusa_dvp_unlock(locked);
    return LISA_DEVICE_OK;
}

/**
 * @brief 打开 DVP MCLK 输出。
 *
 * @param dev LISA 设备对象，当前未直接使用。
 * @param clock MCLK 输出频率，单位 Hz。
 * @return LISA_DEVICE_OK 打开成功，否则返回 LISA 设备错误码。
 */
static int venusa_dvp_enable_clockout(const lisa_device_t *dev, uint32_t clock)
{
    (void)dev;

    bool locked = venusa_dvp_lock();

    if (!dvp_priv.initialized) {
        LISA_LOGE(LOG_TAG, "DVP not initialized");
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    int32_t ret = DVP_EnableClockout(clock);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "DVP_EnableClockout failed: %d", ret);
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_IO;
    }

    dvp_priv.clockout_enabled = true;
    LISA_LOGI(LOG_TAG, "DVP clockout enabled successfully, freq: %u Hz", clock);
    venusa_dvp_unlock(locked);
    return LISA_DEVICE_OK;
}

/**
 * @brief 普通模式下重新装载目标帧缓冲区。
 *
 * @param dev LISA 设备对象，当前未直接使用。
 * @param buf 新目标帧缓冲区地址。
 * @param len 新目标帧缓冲区长度，单位字节。
 * @return LISA_DEVICE_OK 装载成功，否则返回 LISA 设备错误码。
 */
static int venusa_dvp_reload(const lisa_device_t *dev, void *buf, uint32_t len)
{
    (void)dev;

    if (!venusa_dvp_buffer_valid(buf, len)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    bool locked = venusa_dvp_lock();

    if (!dvp_priv.initialized) {
        LISA_LOGE(LOG_TAG, "DVP not initialized");
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (dvp_priv.stop_flag || dvp_priv.mode != LISA_DVP_VENUSA_MODE_NORMAL) {
        LISA_LOGW(LOG_TAG, "DVP not running in normal mode");
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    int ret = venusa_dvp_dma_start_locked(buf, len);
    if (ret != LISA_DEVICE_OK) {
        dvp_priv.stop_flag = true;
        DVP_Stop(dvp_priv.hal_handler);
        venusa_dvp_dma_stop(true);
    }

    venusa_dvp_unlock(locked);
    return ret;
}

/**
 * @brief Ping-Pong 模式下更新最近完成的缓冲区地址。
 *
 * @param dev LISA 设备对象，当前未直接使用。
 * @param buf 新的 Ping 或 Pong 缓冲区地址。
 * @return LISA_DEVICE_OK 更新成功，否则返回 LISA 设备错误码。
 */
static int venusa_dvp_reload_pingpong(const lisa_device_t *dev, void *buf)
{
    (void)dev;

    if (!buf) {
        return LISA_DEVICE_ERR_INVALID;
    }

    bool locked = venusa_dvp_lock();

    if (!dvp_priv.initialized) {
        LISA_LOGE(LOG_TAG, "DVP not initialized");
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (dvp_priv.stop_flag || dvp_priv.mode != LISA_DVP_VENUSA_MODE_PINGPONG || dvp_priv.completed_slot < 0) {
        LISA_LOGW(LOG_TAG, "DVP Ping-Pong not ready for reload");
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (!venusa_dvp_buffer_valid(buf, dvp_priv.pingpong_len)) {
        venusa_dvp_unlock(locked);
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = LISA_DEVICE_OK;
#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    venusa_dvp_cache_invalidate(buf, dvp_priv.pingpong_len);

    DMA_PIPO_BLK block = {
        .src = (void *)(uintptr_t)DVP0_Buf(),
        .dst = buf,
        .size = venusa_dvp_dma_words(dvp_priv.pingpong_len),
        .flags = 0,
    };
    uint8_t block_count = 1;
    int32_t hal_ret = CSK_DRIVER_OK;
#endif
    uint8_t gint = venusa_dvp_isr_disable();

    int8_t slot = dvp_priv.completed_slot;
    if (dvp_priv.stop_flag || slot < 0) {
        ret = LISA_DEVICE_ERR_NOT_READY;
    } else {
#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
        hal_ret = dma_channel_start_pipo(dvp_priv.dma_ch, &block, &block_count);
        if (hal_ret != CSK_DRIVER_OK || block_count == 0) {
            ret = LISA_DEVICE_ERR_IO;
        } else
#endif
        {
            if (slot == 0) {
                dvp_priv.ping_buf = buf;
            } else {
                dvp_priv.pong_buf = buf;
            }
            dvp_priv.completed_slot = -1;
        }
    }

    venusa_dvp_isr_restore(gint);

#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    if (hal_ret != CSK_DRIVER_OK || block_count == 0) {
        LISA_LOGE(LOG_TAG, "dma_channel_start_pipo reload failed: %d, blocks=%u", hal_ret, block_count);
    }
#endif

    if (ret == LISA_DEVICE_ERR_NOT_READY) {
        LISA_LOGW(LOG_TAG, "DVP Ping-Pong not ready for reload");
    }

    venusa_dvp_unlock(locked);
    return ret;
}

/**
 * @brief Venusa DVP LISA API 虚表。
 */
static const lisa_dvp_api_t venusa_dvp_api = {
    .setup = venusa_dvp_setup,
    .stop = venusa_dvp_stop,
    .start = venusa_dvp_start,
    .reload = venusa_dvp_reload,
    .start_pingpong = venusa_dvp_start_pingpong,
    .reload_pingpong = venusa_dvp_reload_pingpong,
    .enable_clockout = venusa_dvp_enable_clockout,
};

/**
 * @brief 创建驱动运行期 OS 资源。
 *
 * @param priv DVP 私有上下文。
 * @return LISA_DEVICE_OK 创建成功，否则返回 LISA 设备错误码。
 */
static int venusa_dvp_init_resources(lisa_dvp_priv_t *priv)
{
    priv->lock = lisa_mutex_create();
    if (!priv->lock) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    return LISA_DEVICE_OK;
}

/**
 * @brief 初始化或复位 DVP 私有硬件状态字段。
 *
 * @param priv DVP 私有上下文。
 * @return LISA_DEVICE_OK 固定返回成功。
 *
 * @note 本函数不创建 mutex，也不调用 HAL 初始化，便于启动和 PM resume 复用。
 */
static int venusa_dvp_init_hw(lisa_dvp_priv_t *priv)
{
    priv->hal_handler = NULL;
    priv->callback = NULL;
    priv->user_data = NULL;
    priv->initialized = false;
    priv->clockout_enabled = false;
    priv->dma_ch = 0;
#if CONFIG_LISA_DVP_VENUSA_USE_CMNDMA
    priv->cmndma_reserved = false;
#endif
    priv->stop_flag = true;
    priv->active_buf = NULL;
    priv->active_len = 0;
    priv->mode = LISA_DVP_VENUSA_MODE_NORMAL;
    priv->ping_buf = NULL;
    priv->pong_buf = NULL;
    priv->pingpong_len = 0;
    priv->active_slot = -1;
    priv->completed_slot = -1;
    return LISA_DEVICE_OK;
}

/**
 * @brief Venusa DVP 设备注册阶段初始化入口。
 *
 * @return LISA_DEVICE_OK 初始化成功，否则返回 LISA 设备错误码。
 */
static int venusa_dvp_init(void)
{
    memset(&dvp_priv, 0, sizeof(dvp_priv));
    int ret = venusa_dvp_init_resources(&dvp_priv);
    if (ret) {
        return ret;
    }
    ret = venusa_dvp_init_hw(&dvp_priv);
    if (ret) {
        return ret;
    }

    LISA_LOGI(LOG_TAG, "DVP driver initialized");
    return LISA_DEVICE_OK;
}

/**
 * @brief 停止并释放 DVP 设备的全部软硬件资源，恢复芯片上电初始状态。
 *
 * 由 lisa_device_destroy() 调用。释放顺序与 venusa_dvp_init 申请顺序相反：
 *   1) 取 lock，复用 venusa_dvp_release_locked 完成 HAL 拆卸（必要时先 DVP_Stop +
 *      DMA2D_Stop、关闭 clockout，再 DVP_Uninitialize）并清业务字段；
 *   2) 释放并删除 mutex；
 *   3) memset 整个 priv，回到 venusa_dvp_init 之前的零初值。
 *
 * 约定：调用方需保证此时采集已停止、无并发业务在使用本设备。
 */
static int venusa_dvp_deinit(void)
{
    lisa_dvp_priv_t *priv = &dvp_priv;

    if (priv->lock) {
        lisa_mutex_lock(priv->lock, -1);
        venusa_dvp_release_locked();
        lisa_mutex_unlock(priv->lock);
        lisa_mutex_delete(priv->lock);
    } else {
        venusa_dvp_release_locked();
    }

    memset(&dvp_priv, 0, sizeof(dvp_priv));
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_PM
/* ===== System PM 回调 =====
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型：应用在睡眠前
 * 调 lisa_device_destroy(dvp0) 释放全部软硬件资源（含 DVP/DMA2D HAL 拆卸与 mutex），
 * 唤醒后在 PM after_wake 回调中调 lisa_device_reinit(dvp0) 重建到 venusa_dvp_init 后
 * 的状态，并由业务重新 lisa_dvp_setup。因此 prepare_suspend / resume_restore 不再
 * 需要（已被 destroy/reinit 覆盖，且二者运行于 PM 临界区无法做重活）。
 *
 * 仅保留 check_idle：采集运行中（DVP + DMA2D 持续搬运帧数据）阻止系统自动进入轻
 * 睡眠。只读 priv 运行标记，不取 mutex / 不访问 HAL。
 */
static int32_t venusa_dvp_pm_check_idle(void *ctx)
{
    lisa_dvp_priv_t *priv = (lisa_dvp_priv_t *)ctx;
    if (!priv) {
        return 1;
    }
    return (priv->initialized && !priv->stop_flag) ? 0 : 1;
}

/**
 * @brief Venusa DVP System PM 操作集合。
 */
static const lisa_pm_system_ops_t venusa_dvp_pm_ops = {
    .check_idle = venusa_dvp_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore = NULL,
};
#endif


// clang-format off
LISA_DEVICE_REGISTER_DEINIT(dvp0, &venusa_dvp_api, &dvp_priv, NULL, venusa_dvp_init,
                            venusa_dvp_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(dvp0, &venusa_dvp_pm_ops, NULL, &dvp_priv);
#endif
// clang-format on
