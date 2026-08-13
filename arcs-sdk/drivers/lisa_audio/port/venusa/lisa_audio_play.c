/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "lisa_audio_play"

#include <string.h>
#include "lisa_audio_internal.h"
#include "audio_dac_init.h"
#include "lisa_log.h"
#include "Driver_DAC.h"
#include "Driver_Common.h"
#include "dma.h"
#include "cache.h"
#include "systick.h"
#include "lisa_mem.h"
#ifdef CONFIG_LISA_AUDIO_PLAY_PA_ENABLE
#include "lisa_gpio.h"
#endif

/* Must be defined in lisa_audio_venusa.c */
extern int audio_submit_event_from_isr(internal_audio_event_t *event);

/* DMA 通道定义 (通过 Kconfig 配置) */
#define GPDMA_DAC0_CHN  CONFIG_LISA_AUDIO_PLAY_DMA_CHN
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
#define GPDMA_ECHO_CHN        CONFIG_LISA_AUDIO_PLAY_ECHO_DMA_CHN
#endif

#if defined(CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE) || defined(CONFIG_LISA_AUDIO_PLAY_SOFT_ECHO)
#define ECHO_BUFFER_COUNT     CONFIG_LISA_AUDIO_RECORD_BUFFER_COUNT
#define ECHO_BUFFER_SAMPLES   CONFIG_LISA_AUDIO_RECORD_BUFFER_SAMPLES
#endif

/* PA 控制配置 */
#ifdef CONFIG_LISA_AUDIO_PLAY_PA_ENABLE
#define PA_PIN_NUM      CONFIG_LISA_AUDIO_PLAY_PA_PIN
#define PA_PULSE_COUNT  CONFIG_LISA_AUDIO_PLAY_PA_PULSE_COUNT
#define PA_PULSE_US     CONFIG_LISA_AUDIO_PLAY_PA_PULSE_US
#if CONFIG_LISA_AUDIO_PLAY_PA_PAD == 0
#define PA_GPIO_DEV_NAME "gpioa"
#else
#define PA_GPIO_DEV_NAME "gpiob"
#endif

static bool pa_initialized = false;
static lisa_device_t *pa_gpio_dev = NULL;
#endif

/* Play 事件标志 */
#define PLAY_EVT_DONE    (1 << 0)

/* 音频格式常量 */
#define STEREO_CHANNELS     (2)
#define MONO_CHANNELS       (1)
#define BYTES_PER_SAMPLE_16 (2)

/* 前向声明 */
static void play_event_callback(uint32_t event, uint32_t user);

static inline uint32_t channel_to_bitmap(lisa_audio_channel_t channels)
{
    return (uint32_t)channels;
}

/* DAC 采样率转换（支持 8K/16K/24K/32K/48K/96K） */
static uint32_t play_sample_rate_to_ctrl(lisa_audio_rate_t rate)
{
    switch (rate) {
    case LISA_AUDIO_RATE_8K:  return CSK_DAC_SR_8KHZ;
    case LISA_AUDIO_RATE_16K: return CSK_DAC_SR_16KHZ;
    case LISA_AUDIO_RATE_24K: return CSK_DAC_SR_24KHZ;
    case LISA_AUDIO_RATE_32K: return CSK_DAC_SR_32KHZ;
    case LISA_AUDIO_RATE_48K: return CSK_DAC_SR_48KHZ;
    case LISA_AUDIO_RATE_96K: return CSK_DAC_SR_96KHZ;
    default: return CSK_DAC_SR_16KHZ;
    }
}

static uint32_t play_get_osr_for_rate(lisa_audio_rate_t rate)
{
    /* ≤24kHz 使用 OSR_250, >24kHz 使用 OSR_125 */
    if (rate <= LISA_AUDIO_RATE_24K) {
        return CSK_DAC_OSR_250;
    } else {
        return CSK_DAC_OSR_125;
    }
}

/* ===== PA 控制 ===== */

#ifdef CONFIG_LISA_AUDIO_PLAY_PA_ENABLE
static int play_pa_gpio_init(void)
{
    pa_gpio_dev = lisa_device_get(PA_GPIO_DEV_NAME);
    if (!pa_gpio_dev) {
        LOGE("PA GPIO device not found: %s", PA_GPIO_DEV_NAME);
        return LISA_DEVICE_ERR_NOT_READY;
    }
    if (lisa_gpio_configure(pa_gpio_dev,
                            PA_PIN_NUM,
                            LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW) != LISA_DEVICE_OK) {
        LOGE("PA GPIO configure failed: dev=%s pin=%d", PA_GPIO_DEV_NAME, PA_PIN_NUM);
        return LISA_DEVICE_ERR_IO;
    }

    return LISA_DEVICE_OK;
}

static void play_pa_gpio_write(uint32_t value)
{
    lisa_gpio_write_pin(pa_gpio_dev, PA_PIN_NUM, value ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
}

static void play_pa_control(bool enable)
{
    /* 初始化GPIO(仅一次) */
    if (!pa_initialized) {
        if (play_pa_gpio_init() != LISA_DEVICE_OK) {
            return;
        }
        pa_initialized = true;
    }
    if (!pa_gpio_dev) {
        return;
    }

    if (enable) {
        /* PA使能:发送脉冲序列(如果配置) */
        if (PA_PULSE_COUNT > 0) {
            for (volatile int i = 0; i < PA_PULSE_COUNT; i++) {
                play_pa_gpio_write(0);
                SysTick_Delay_Us(PA_PULSE_US);
                play_pa_gpio_write(1);
                SysTick_Delay_Us(PA_PULSE_US);
            }
        } else {
            /* 无脉冲模式:直接拉高 */
            play_pa_gpio_write(1);
        }
    } else {
        /* PA关闭:拉低并延迟 */
        play_pa_gpio_write(0);
        SysTick_Delay_Us(PA_PULSE_US);
    }
}
#endif

/* ===== Play 事件回调辅助函数 ===== */

static inline bool get_next_play_buffer(lisa_audio_play_priv_t *priv,
                                        play_item_t *item,
                                        bool in_isr,
                                        BaseType_t *yield)
{
    BaseType_t ok = in_isr
                  ? xQueueReceiveFromISR(priv->play_queue, item, yield)
                  : xQueueReceive(priv->play_queue, item, 0);
    if (ok == pdPASS) {
        return true;
    }

    /* 队列空，使用空闲 buffer 填充静音 */
    ok = in_isr
       ? xQueueReceiveFromISR(priv->free_queue, item, yield)
       : xQueueReceive(priv->free_queue, item, 0);
    if (ok == pdPASS) {
        memset(item->addr, 0, priv->buffer_size);
        return true;
    }

    /* 极端情况：无可用 buffer (buffer_count 配置过小) */
    item->addr = NULL;
    return false;
}

static inline int submit_buffer_to_dac(lisa_audio_play_priv_t *priv, const play_item_t *item)
{
    HAL_FlushDCache_by_Addr(item->addr, priv->buffer_size);

    uint32_t channel_bitmap = channel_to_bitmap(LISA_AUDIO_CH_LEFT);

    return DAC_Send(priv->hdrv,
                    item->addr,
                    priv->buffer_samples,
                    channel_bitmap,
                    DAC_TX_FLAG_START_NOW);
}

static inline void recycle_completed_buffer(lisa_audio_play_priv_t *priv,
                                           void *completed_addr,
                                           bool in_isr,
                                           BaseType_t *yield)
{
    if (completed_addr) {
        play_item_t recycled = {.addr = completed_addr, .size = 0};
        if (in_isr) {
            xQueueSendToBackFromISR(priv->free_queue, &recycled, yield);
        } else {
            xQueueSendToBack(priv->free_queue, &recycled, 0);
        }
    }
}

/* ===== Play 事件回调 ===== */

static void play_event_callback(uint32_t event, uint32_t user)
{
    lisa_audio_play_priv_t *priv = (lisa_audio_play_priv_t *)user;
    const bool in_isr = (xPortIsInsideInterrupt() == pdTRUE);
    BaseType_t yield = pdFALSE;
    const bool play_active = (priv->state == PLAY_STATE_PLAY_REQ ||
                              priv->state == PLAY_STATE_PLAY_RUN);

    if (!play_active) {
        return;
    }

    if (event & CSK_DAC_EVENT_SEND_COMPLETE) {
        void *completed_addr = priv->active_addr;
        play_item_t next_item = {.addr = NULL, .size = 0};
        bool submitted = false;

        if (priv->state == PLAY_STATE_PLAY_REQ) {
#ifdef CONFIG_LISA_AUDIO_PLAY_PA_ENABLE
            play_pa_control(true);
#endif
            priv->state = PLAY_STATE_PLAY_RUN;
        }

        if (priv->state == PLAY_STATE_PLAY_RUN) {
            if (get_next_play_buffer(priv, &next_item, in_isr, &yield) && next_item.addr) {
                priv->active_addr = next_item.addr;
                int ret = submit_buffer_to_dac(priv, &next_item);
                if (ret == CSK_DRIVER_OK) {
                    submitted = true;
                } else {
                    LOGE("DAC_Send refill failed: %d", ret);
                    recycle_completed_buffer(priv, next_item.addr, in_isr, &yield);
                    priv->active_addr = NULL;
                }
            }
        }

#ifdef CONFIG_LISA_AUDIO_PLAY_SOFT_ECHO
        if (completed_addr && priv->state == PLAY_STATE_PLAY_RUN) {
            void *echo_buf = priv->echo_fifo[priv->echo_xpos];
            uint32_t copy_size = (priv->buffer_size < priv->echo_buffer_size)
                                     ? priv->buffer_size
                                     : priv->echo_buffer_size;
            memcpy(echo_buf, completed_addr, copy_size);
            if (copy_size < priv->echo_buffer_size) {
                memset((uint8_t *)echo_buf + copy_size, 0, priv->echo_buffer_size - copy_size);
            }

            internal_audio_event_t echo_event = {
                .type = AUDIO_EVENT_TYPE_ECHO,
                .buffer = echo_buf,
                .samples = priv->echo_buffer_samples,
                .timestamp = SysTimeMsGet() * 1000000ULL,
            };
            if (audio_submit_event_from_isr(&echo_event) == 0) {
                if (++priv->echo_xpos >= priv->echo_buffer_count) {
                    priv->echo_xpos = 0;
                }
            }
        }
#endif

        recycle_completed_buffer(priv, completed_addr, in_isr, &yield);
        if (!submitted && priv->state == PLAY_STATE_PLAY_RUN) {
            priv->active_addr = NULL;
            priv->state = PLAY_STATE_IDLE;
            priv->status = LISA_AUDIO_STATUS_IDLE;
            if (in_isr) {
                xEventGroupSetBitsFromISR(priv->event, PLAY_EVT_DONE, &yield);
            } else {
                xEventGroupSetBits(priv->event, PLAY_EVT_DONE);
            }
        }
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_EMPTY) {
        LOGW("FIFO empty");
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_UNDERRUN) {
        LOGE("FIFO underrun");
    }

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    if (event & CSK_DAC_EVENT_ECHO_RX_COMPLETE) {
        int ret = CSK_DRIVER_OK;
        void *recv = priv->echo_fifo[priv->echo_xpos];

        dcache_invalidate_range((uint32_t)recv, (uint32_t)recv + priv->echo_buffer_size);

        /* 先尝试提交事件，只有成功时才更新索引，避免数据错乱 */
        internal_audio_event_t new_event = {
            .type = AUDIO_EVENT_TYPE_ECHO,
            .buffer = recv,
            .samples = priv->echo_buffer_samples,
            .timestamp = SysTimeMsGet() * 1000000ULL,
        };
        int submit_ret = audio_submit_event_from_isr(&new_event);

        /* 只有在事件成功提交后才更新 echo_xpos */
        if (submit_ret == 0) {
            if (++priv->echo_xpos >= priv->echo_buffer_count) {
                priv->echo_xpos = 0;
            }
        } else {
            /* 事件提交失败，保持 echo_xpos 不变，下次继续使用同一个 buffer */
            LOGW("Echo event submit failed, reusing buffer %d", priv->echo_xpos);
        }

        ret = DAC_Echo_Receive(priv->hdrv,
                               priv->echo_fifo[priv->echo_xpos],
                               priv->echo_buffer_samples,
                               channel_to_bitmap(LISA_AUDIO_CH_LEFT));
        if (CSK_DRIVER_OK != ret){
            LOGE("DAC_Echo_Receive:%d", ret);
        }
    }
#endif

    if (in_isr) {
        portYIELD_FROM_ISR(yield);
    }
}

static void venusa_audio_play_release_runtime(lisa_audio_play_priv_t *priv)
{
    if (priv == NULL) {
        return;
    }

    if (priv->play_queue) vQueueDelete(priv->play_queue);
    if (priv->free_queue) vQueueDelete(priv->free_queue);
    if (priv->event) vEventGroupDelete(priv->event);
    if (priv->buffer_pool) lisa_mem_free(priv->buffer_pool);
#if defined(CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE) || defined(CONFIG_LISA_AUDIO_PLAY_SOFT_ECHO)
    if (priv->echo_fifo) {
        if (priv->echo_fifo[0]) {
            lisa_mem_free(priv->echo_fifo[0]);
        }
        lisa_mem_free(priv->echo_fifo);
    }
#endif

    priv->play_queue = NULL;
    priv->free_queue = NULL;
    priv->event = NULL;
    priv->buffer_pool = NULL;
#if defined(CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE) || defined(CONFIG_LISA_AUDIO_PLAY_SOFT_ECHO)
    priv->echo_fifo = NULL;
#endif
}

int venusa_audio_play_config(lisa_audio_play_priv_t *priv, const lisa_audio_play_config_t *config)
{
    int ret = 0;

    if (!config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 播放启动后不允许重新配置 */
    if (priv->state != PLAY_STATE_IDLE) {
        LOGE("Cannot reconfigure play after start");
        return LISA_DEVICE_ERR_INVALID;
    }

    memcpy(&priv->config, config, sizeof(lisa_audio_play_config_t));

#ifdef CONFIG_LISA_AUDIO_PLAY_PA_ENABLE
    /* 确保PA处于关闭状态 */
    play_pa_control(false);
#endif

    /* 注意: 即使不启用Echo,也必须设置echo通道,否则会导致杂音 */
    DAC_DMA_CHS dmach;
    memset(&dmach, 0xFF, sizeof(dmach));
    dmach.dma_ch_out_left = GPDMA_DAC0_CHN;
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    dmach.dma_ch_echo_left = GPDMA_ECHO_CHN;
#endif

    DAC_Uninitialize(priv->hdrv);

    /* 强制使用单声道配置底层硬件 */
    uint32_t hw_channels = LISA_AUDIO_CH_LEFT;
    uint32_t flags = (channel_to_bitmap(hw_channels) << DAC_BMP_FLAG_OUT_POS) |
                     DAC_BMP_FLAG_USE_16BITS;
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    flags |= (channel_to_bitmap(hw_channels) << DAC_BMP_FLAG_ECHO_POS);
#endif

    ret = DAC_Initialize(priv->hdrv,
                         play_event_callback,
                         (uint32_t)priv,
                         flags,
                         &dmach);
    if (ret != 0) {
        LOGE("DAC_Initialize failed: %d", ret);
        goto exit;
    }

    ret = DAC_PowerControl(priv->hdrv, CSK_POWER_FULL);
    if (ret != 0) {
        LOGE("DAC_PowerControl failed: %d", ret);
        goto exit;
    }

    uint32_t sr_ctrl = play_sample_rate_to_ctrl(config->format.sample_rate);
    uint32_t osr_ctrl = play_get_osr_for_rate(config->format.sample_rate);

    ret = DAC_Control(priv->hdrv,
                      sr_ctrl | osr_ctrl | CSK_DAC_SOFT_MUTE_SET,
                      CSK_DAC_ARG_SOFT_MUTE_EN | CSK_DAC_ARG_SOFT_MUTE_SPD(3));
    if (ret != 0) {
        LOGE("DAC_Control failed: %d", ret);
        goto exit;
    }

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    ECHO_PARAMS echo_params = { 0 };
    echo_params.echo_mixed = 0; // 1; // only 1 ECHO channel for only 1 DAC channel
    echo_params.samp_rate = config->format.sample_rate;
    echo_params.trim_16bits = 1; // 16bits echo?
    ret = DAC_Control(priv->hdrv, CSK_DAC_SET_ECHO_PARAMS, (uint32_t)&echo_params);
    if (CSK_DRIVER_OK != ret){
        LOGE("DAC_Control for ECHO failed: %d", ret);
        goto exit;
    }
#endif

    uint32_t dev_bitmap = channel_to_bitmap(hw_channels);
    ret = DAC_SetMute(priv->hdrv, dev_bitmap, dev_bitmap);
    if (ret != 0) {
        LOGE("DAC_SetMute failed: %d", ret);
        goto exit;
    }

    uint32_t gain_d = DAC_GAIN_D_VAL(config->gain.digital_gain);
    uint32_t vol_flag = 0;
    
    if (hw_channels & LISA_AUDIO_CH_LEFT) {
        vol_flag |= DAC_VOL_FLAG_D_LEFT;
    }
    /* 如果硬件只有左声道，忽略右声道增益配置 */

    ret = DAC_SetVolume(priv->hdrv, gain_d, vol_flag);
    if (ret != 0) {
        LOGE("DAC_SetVolume failed: %d", ret);
        goto exit;
    }

    /* 计算缓冲区参数 (强制单声道输出) */
    priv->buffer_count = config->buffer_count;
    priv->buffer_samples = config->buffer_samples * MONO_CHANNELS;
    priv->buffer_size = priv->buffer_samples * (uint8_t)config->format.sample_bits;

    venusa_audio_play_release_runtime(priv);

    const uint32_t pool_alignment = 32;
    const uint32_t pool_size = priv->buffer_count * priv->buffer_size;
    priv->buffer_pool = lisa_mem_align_alloc(pool_alignment, pool_size);
    if (!priv->buffer_pool) {
        LOGE("Failed to allocate buffer pool: %u bytes", pool_size);
        ret = LISA_DEVICE_ERR_NO_MEM;
        goto exit;
    }

    priv->play_queue = xQueueCreate(priv->buffer_count, sizeof(play_item_t));
    priv->free_queue = xQueueCreate(priv->buffer_count, sizeof(play_item_t));
    priv->event = xEventGroupCreate();

    if (!priv->play_queue || !priv->free_queue || !priv->event) {
        LOGE("Failed to create queues/events");
        ret = LISA_DEVICE_ERR_NO_MEM;
        goto exit;
    }

#if defined(CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE) || defined(CONFIG_LISA_AUDIO_PLAY_SOFT_ECHO)
    priv->echo_buffer_count = ECHO_BUFFER_COUNT;
    priv->echo_buffer_samples = ECHO_BUFFER_SAMPLES;
    priv->echo_buffer_size = priv->echo_buffer_samples * BYTES_PER_SAMPLE_16;

    const uint32_t echo_pool_size = priv->echo_buffer_count * priv->echo_buffer_size;
    void *echo_buffer_pool = lisa_mem_align_alloc(pool_alignment, echo_pool_size);
    if (!echo_buffer_pool) {
        LOGE("Failed to allocate echo buffer pool: %u bytes", echo_pool_size);
        ret = LISA_DEVICE_ERR_NO_MEM;
        goto exit;
    }

    priv->echo_fifo = lisa_mem_alloc(sizeof(void *) * priv->echo_buffer_count);
    if (!priv->echo_fifo) {
        lisa_mem_free(echo_buffer_pool);
        ret = LISA_DEVICE_ERR_NO_MEM;
        goto exit;
    }
    priv->echo_fifo[0] = echo_buffer_pool;
    for (int i = 1; i < priv->echo_buffer_count; i++) {
        priv->echo_fifo[i] = (uint8_t *)echo_buffer_pool + i * priv->echo_buffer_size;
    }
    priv->echo_xpos = 0;
#endif

    for (int i = 0; i < priv->buffer_count; i++) {
        play_item_t item = {
            .addr = priv->buffer_pool + i * priv->buffer_size,
            .size = 0
        };
        xQueueSendToBack(priv->free_queue, &item, 0);
    }

    priv->active_addr = NULL;
    priv->state = PLAY_STATE_IDLE;
    priv->status = LISA_AUDIO_STATUS_IDLE;

exit:
    return ret;
}


static void audio_downmix_stereo_to_mono_16bit(const int16_t *src, int16_t *dst, uint32_t frames)
{
    for (uint32_t i = 0; i < frames; i++) {
        int32_t left = src[STEREO_CHANNELS * i];
        int32_t right = src[STEREO_CHANNELS * i + 1];
        dst[i] = (int16_t)((left + right) / 2);
    }
}

static inline void pad_buffer_with_silence(void *buffer, uint32_t used_size, uint32_t total_size)
{
    if (used_size < total_size) {
        memset((uint8_t *)buffer + used_size, 0, total_size - used_size);
    }
}

static inline uint32_t calc_stereo_process_samples(uint32_t max_mono_samples, uint32_t samples_left)
{
    uint32_t max_stereo_samples = max_mono_samples * STEREO_CHANNELS;
    uint32_t samples = (samples_left > max_stereo_samples) ? max_stereo_samples : samples_left;
    /* 确保采样数为偶数 (左右声道配对) */
    return samples & ~1U;
}

static uint32_t process_stereo_data(const int16_t *input,
                                    void *output_buffer,
                                    uint32_t samples_to_process,
                                    uint32_t buffer_size)
{
    uint32_t frames = samples_to_process / STEREO_CHANNELS;
    audio_downmix_stereo_to_mono_16bit(input, (int16_t *)output_buffer, frames);

    uint32_t used_size = frames * BYTES_PER_SAMPLE_16;
    pad_buffer_with_silence(output_buffer, used_size, buffer_size);

    return used_size;
}

static uint32_t process_mono_data(const void *input,
                                  void *output_buffer,
                                  uint32_t samples_to_process,
                                  uint32_t bytes_per_sample,
                                  uint32_t buffer_size)
{
    uint32_t copy_size = samples_to_process * bytes_per_sample;
    memcpy(output_buffer, input, copy_size);
    pad_buffer_with_silence(output_buffer, copy_size, buffer_size);

    return copy_size;
}

int venusa_audio_play_write(lisa_audio_play_priv_t *priv, const void *buffer, uint32_t samples)
{
    if (!priv || !buffer || samples == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->free_queue || !priv->play_queue) {
        LOGE("Play runtime is not configured");
        return LISA_DEVICE_ERR_INVALID;
    }

    const TickType_t wait_timeout = portMAX_DELAY;
    const bool is_stereo_input = (priv->config.format.channels == LISA_AUDIO_CH_STEREO);
    const uint32_t input_bytes_per_sample = (uint8_t)priv->config.format.sample_bits;
    const uint32_t max_mono_samples = priv->buffer_size / BYTES_PER_SAMPLE_16;

    const uint8_t *input_ptr = (const uint8_t *)buffer;
    uint32_t remaining_samples = samples;
    uint32_t total_written = 0;

    while (remaining_samples > 0) {
        play_item_t item;
        if (xQueueReceive(priv->free_queue, &item, wait_timeout) != pdPASS) {
            LOGW("Wait free buffer timeout");
            break;
        }

        uint32_t samples_to_process;
        if (is_stereo_input) {
            samples_to_process = calc_stereo_process_samples(max_mono_samples, remaining_samples);
        } else {
            samples_to_process = (remaining_samples > max_mono_samples) ? max_mono_samples : remaining_samples;
        }

        if (samples_to_process == 0) {
            xQueueSendToBack(priv->free_queue, &item, 0);
            break;
        }

        if (is_stereo_input) {
            item.size = process_stereo_data((const int16_t *)input_ptr,
                                           item.addr,
                                           samples_to_process,
                                           priv->buffer_size);
        } else {
            item.size = process_mono_data(input_ptr,
                                         item.addr,
                                         samples_to_process,
                                         input_bytes_per_sample,
                                         priv->buffer_size);
        }

        HAL_FlushDCache_by_Addr(item.addr, priv->buffer_size);

        if (xQueueSendToBack(priv->play_queue, &item, wait_timeout) != pdPASS) {
            xQueueSendToBack(priv->free_queue, &item, 0);
            LOGE("Failed to enqueue buffer");
            break;
        }

        uint32_t consumed_bytes = samples_to_process * input_bytes_per_sample;
        input_ptr += consumed_bytes;
        remaining_samples -= samples_to_process;
        total_written += samples_to_process;
    }

    return total_written;
}

int venusa_audio_play_get_buffer(lisa_audio_play_priv_t *priv, void **buffer, uint32_t timeout_ms)
{
    if (!buffer) {
        return LISA_DEVICE_ERR_INVALID;
    }

    play_item_t item;
    TickType_t ticks = (timeout_ms == 0xFFFFFFFF) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    if (xQueueReceive(priv->free_queue, &item, ticks) == pdPASS) {
        *buffer = item.addr;
        return priv->buffer_samples;
    }

    *buffer = NULL;
    return 0;
}


int venusa_audio_play_control(lisa_audio_play_priv_t *priv, uint32_t cmd, void *arg)
{
    int ret = 0;

    switch (cmd) {
    case LISA_AUDIO_IOCTL_PLAY_START:
        if (priv->state == PLAY_STATE_IDLE) {
            priv->state = PLAY_STATE_PLAY_REQ;

            uint32_t dev_bitmap = channel_to_bitmap(LISA_AUDIO_CH_LEFT);
            play_item_t item1 = {0};

            if (xQueueReceive(priv->play_queue, &item1, 0) != pdPASS) {
                if (xQueueReceive(priv->free_queue, &item1, 0) == pdPASS) {
                    memset(item1.addr, 0, priv->buffer_size);
                }
            }

            if (item1.addr) {
                HAL_FlushDCache_by_Addr(item1.addr, priv->buffer_size);

                priv->active_addr = item1.addr;

                ret = submit_buffer_to_dac(priv, &item1);
                if (ret == 0) {
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
                    priv->echo_xpos = 0;
                    ret = DAC_Echo_Receive(priv->hdrv,
                                           priv->echo_fifo[0],
                                           priv->echo_buffer_samples,
                                           dev_bitmap);
                    if (ret != 0) {
                        LOGE("DAC_Echo_Receive failed: %d", ret);
                    }
#elif defined(CONFIG_LISA_AUDIO_PLAY_SOFT_ECHO)
                    priv->echo_xpos = 0;
#endif
                    DAC_SetMute(priv->hdrv, 0, dev_bitmap);
                    priv->status = LISA_AUDIO_STATUS_RUNNING;
                } else {
                    LOGE("DAC_Send failed: %d", ret);
                    priv->state = PLAY_STATE_IDLE;
                    xQueueSendToBack(priv->free_queue, &item1, 0);
                }
            } else {
                LOGE("Failed to get start buffers");
                priv->state = PLAY_STATE_IDLE;
                if (item1.addr) xQueueSendToBack(priv->free_queue, &item1, 0);
            }
        }

        break;

    case LISA_AUDIO_IOCTL_PLAY_STOP:
    {
        uint32_t dev_bitmap = channel_to_bitmap(LISA_AUDIO_CH_LEFT);
        uint32_t echo_bitmap = 0;
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
        echo_bitmap = channel_to_bitmap(LISA_AUDIO_CH_LEFT);
#endif

        priv->state = PLAY_STATE_STOP_REQ;
        priv->status = LISA_AUDIO_STATUS_IDLE;

        DAC_SetMute(priv->hdrv, dev_bitmap, dev_bitmap);
#ifdef CONFIG_LISA_AUDIO_PLAY_PA_ENABLE
        if (pa_initialized) {
            play_pa_control(false);
        }
#endif
        DAC_Abort(priv->hdrv, dev_bitmap, echo_bitmap);
        priv->state = PLAY_STATE_IDLE;

        if (priv->play_queue && priv->free_queue) {
            play_item_t item;
            while (xQueueReceive(priv->play_queue, &item, 0) == pdPASS) {
                xQueueSendToBack(priv->free_queue, &item, 0);
            }

            if (priv->active_addr) {
                item.addr = priv->active_addr;
                item.size = 0;
                xQueueSendToBack(priv->free_queue, &item, 0);
                priv->active_addr = NULL;
            }
        } else {
            priv->active_addr = NULL;
        }

        break;
    }

    case LISA_AUDIO_IOCTL_PLAY_SET_GAIN:
        if (arg) {
            lisa_audio_gain_t *gain = (lisa_audio_gain_t *)arg;
            uint32_t gain_d = DAC_GAIN_D_VAL(gain->digital_gain);
            uint32_t vol_flag = DAC_VOL_FLAG_D_LEFT;

            ret = DAC_SetVolume(priv->hdrv, gain_d, vol_flag);
        }
        break;

    case LISA_AUDIO_IOCTL_PLAY_FLUSH:
        if (priv->state == PLAY_STATE_PLAY_RUN) {
            while (uxQueueMessagesWaiting(priv->play_queue) > 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            /* 等待最后一个buffer播完 (估算时间) */
            uint32_t buffer_ms = priv->buffer_samples * 1000 / priv->config.format.sample_rate;
            vTaskDelay(pdMS_TO_TICKS(buffer_ms + 10));
        }
        break;

    case LISA_AUDIO_IOCTL_PLAY_GET_STATUS:
        if (arg) {
            *(lisa_audio_status_t *)arg = priv->status;
        }
        break;

    default:
        ret = LISA_DEVICE_ERR_NOT_SUPPORT;
        break;
    }

    return ret;
}

int venusa_audio_play_init(lisa_audio_play_priv_t *priv)
{
    memset(priv, 0, sizeof(lisa_audio_play_priv_t));

    audio_dac_platform_init();

    priv->hdrv = DAC01();
    priv->status = LISA_AUDIO_STATUS_IDLE;
    priv->state = PLAY_STATE_IDLE;

    priv->initialized = true;

    return LISA_DEVICE_OK;
}

int venusa_audio_play_deinit(lisa_audio_play_priv_t *priv)
{
    if (priv == NULL) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 停止播放：静音、关 PA、停 DMA，状态回 IDLE（与 PLAY_STOP 路径一致） */
    if (priv->hdrv && priv->state != PLAY_STATE_IDLE) {
        uint32_t dev_bitmap = channel_to_bitmap(LISA_AUDIO_CH_LEFT);
        uint32_t echo_bitmap = 0;
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
        echo_bitmap = channel_to_bitmap(LISA_AUDIO_CH_LEFT);
#endif
        DAC_SetMute(priv->hdrv, dev_bitmap, dev_bitmap);
#ifdef CONFIG_LISA_AUDIO_PLAY_PA_ENABLE
        if (pa_initialized) {
            play_pa_control(false);
        }
#endif
        DAC_Abort(priv->hdrv, dev_bitmap, echo_bitmap);
        priv->state = PLAY_STATE_IDLE;
    }

    /* 释放运行期 OS / 堆资源（play_queue / free_queue / event / buffer_pool / echo_fifo） */
    venusa_audio_play_release_runtime(priv);

    /* 关闭 DAC：先 PowerControl(OFF) 再 Uninitialize，使硬件回到上电初始态 */
    if (priv->hdrv) {
        DAC_PowerControl(priv->hdrv, CSK_POWER_OFF);
        DAC_Uninitialize(priv->hdrv);
    }

    priv->status = LISA_AUDIO_STATUS_IDLE;
    priv->state = PLAY_STATE_IDLE;
    priv->active_addr = NULL;
    priv->initialized = false;

    return LISA_DEVICE_OK;
}
