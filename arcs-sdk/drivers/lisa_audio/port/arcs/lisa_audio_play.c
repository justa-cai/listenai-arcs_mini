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
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "systick.h"
#include "lisa_mem.h"

/* Must be defined in lisa_audio_arcs.c */
extern int audio_submit_event_from_isr(internal_audio_event_t *event);

/* DMA 通道定义 */
#define GPDMA_DAC0_CHN  (2)
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
#define GPDMA_ECHO_CHN        (3)
#define ECHO_BUFFER_COUNT     CONFIG_LISA_AUDIO_RECORD_BUFFER_COUNT
#define ECHO_BUFFER_SAMPLES   CONFIG_LISA_AUDIO_RECORD_BUFFER_SAMPLES
#endif

/* PA 控制配置 */
#ifdef CONFIG_LISA_AUDIO_PLAY_PA_ENABLE
#if CONFIG_LISA_AUDIO_PLAY_PA_PAD == 0
#define PA_GPIO_PAD     CSK_IOMUX_PAD_A
#define PA_GPIO_DEV     GPIOA()
#else
#define PA_GPIO_PAD     CSK_IOMUX_PAD_B
#define PA_GPIO_DEV     GPIOB()
#endif

#define PA_PIN_NUM      CONFIG_LISA_AUDIO_PLAY_PA_PIN
#define PA_GPIO_PIN     (1 << PA_PIN_NUM)
#define PA_PULSE_COUNT  CONFIG_LISA_AUDIO_PLAY_PA_PULSE_COUNT
#define PA_PULSE_US     CONFIG_LISA_AUDIO_PLAY_PA_PULSE_US
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
static void play_pa_control(bool enable)
{
    static bool pa_initialized = false;

    /* 初始化GPIO(仅一次) */
    if (!pa_initialized) {
        IOMuxManager_PinConfigure(PA_GPIO_PAD, PA_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT);
        GPIO_Initialize(PA_GPIO_DEV, NULL, NULL);
        GPIO_Control(PA_GPIO_DEV, CSK_GPIO_DEBOUNCE_DISABLE, PA_GPIO_PIN);
        GPIO_SetDir(PA_GPIO_DEV, PA_GPIO_PIN, CSK_GPIO_DIR_OUTPUT);
        pa_initialized = true;
    }

    if (enable) {
        /* PA使能:发送脉冲序列(如果配置) */
        if (PA_PULSE_COUNT > 0) {
            for (volatile int i = 0; i < PA_PULSE_COUNT; i++) {
                GPIO_PinWrite(PA_GPIO_DEV, PA_GPIO_PIN, 0);
                SysTick_Delay_Us(PA_PULSE_US);
                GPIO_PinWrite(PA_GPIO_DEV, PA_GPIO_PIN, 1);
                SysTick_Delay_Us(PA_PULSE_US);
            }
        } else {
            /* 无脉冲模式:直接拉高 */
            GPIO_PinWrite(PA_GPIO_DEV, PA_GPIO_PIN, 1);
        }
    } else {
        /* PA关闭:拉低并延迟 */
        GPIO_PinWrite(PA_GPIO_DEV, PA_GPIO_PIN, 0);
        SysTick_Delay_Us(PA_PULSE_US);
    }
}
#endif

/* ===== Play 事件回调辅助函数 ===== */

static inline bool get_next_play_buffer(lisa_audio_play_priv_t *priv,
                                        play_item_t *item,
                                        BaseType_t *yield)
{
    if (xQueueReceiveFromISR(priv->play_queue, item, yield) == pdPASS) {
        return true;
    }

    /* 队列空，使用空闲 buffer 填充静音 */
    if (xQueueReceiveFromISR(priv->free_queue, item, yield) == pdPASS) {
        memset(item->addr, 0, priv->buffer_size);
        return true;
    }

    /* 极端情况：无可用 buffer (buffer_count 配置过小) */
    item->addr = NULL;
    return false;
}

static inline void submit_buffer_to_dac(lisa_audio_play_priv_t *priv, const play_item_t *item)
{
    HAL_FlushDCache_by_Addr(item->addr, priv->buffer_size);

    PIPO_OUT_BLOCK block = {
        .sample_data = item->addr,
        .sample_cnt = priv->buffer_samples,
        .flags = 0
    };

    uint8_t block_count = 1;
    uint32_t channel_bitmap = channel_to_bitmap(priv->config.format.channels);

    DAC_Send_PiPo(priv->hdrv, &block, &block_count, channel_bitmap, DAC_TX_FLAG_START_NOW);
}

static inline void recycle_completed_buffer(lisa_audio_play_priv_t *priv,
                                           void *completed_addr,
                                           BaseType_t *yield)
{
    if (completed_addr) {
        play_item_t recycled = {.addr = completed_addr, .size = 0};
        xQueueSendToBackFromISR(priv->free_queue, &recycled, yield);
    }
}

/* ===== Play 事件回调 ===== */

static void play_event_callback(uint32_t event, uint32_t user)
{
    lisa_audio_play_priv_t *priv = (lisa_audio_play_priv_t *)user;
    BaseType_t yield = pdFALSE;

    if (event & CSK_DAC_EVENT_SEND_COMPLETE) {
        priv->state = PLAY_STATE_IDLE;
        xEventGroupSetBitsFromISR(priv->event, PLAY_EVT_DONE, &yield);
    }

    /* 处理 Ping-Pong buffer 切换事件 */
    if (event & (CSK_DAC_EVENT_BLOCK_COMPLETE | CSK_DAC_EVENT_SEND_COMPLETE)) {
        play_item_t next_item = {.addr = NULL, .size = 0};
        void *completed_addr = NULL;

        if (priv->state == PLAY_STATE_PLAY_REQ) {
#ifdef CONFIG_LISA_AUDIO_PLAY_PA_ENABLE
            /* 第一个静音块完成，DAC 输出稳定，启用 PA */
            play_pa_control(true);
#endif
            priv->state = PLAY_STATE_PLAY_RUN;
        }

        if (priv->state == PLAY_STATE_PLAY_RUN) {
            if (get_next_play_buffer(priv, &next_item, &yield)) {
                if (next_item.addr) {
                    submit_buffer_to_dac(priv, &next_item);
                }
            }
        }

        if (event & CSK_DAC_EVENT_TX_PING_DONE) {
            completed_addr = priv->ping_addr;
            priv->ping_addr = next_item.addr;
        } else if (event & CSK_DAC_EVENT_TX_PONG_DONE) {
            completed_addr = priv->pong_addr;
            priv->pong_addr = next_item.addr;
        }

        recycle_completed_buffer(priv, completed_addr, &yield);
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_EMPTY) {
        LOGW("FIFO empty");
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_UNDERRUN) {
        LOGE("FIFO underrun");
    }

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    if (event & (CSK_DAC_EVENT_ECHO_RX_COMPLETE | CSK_DAC_EVENT_ECHO_BLOCK_COMPLETE)) {
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

        /* 计算下一个用于 DMA 的 buffer 索引 */
        int ipos = priv->echo_xpos;
        if (++ipos >= priv->echo_buffer_count) {
            ipos = 0;
        }

        ret = DAC_Echo_Receive_PiPo(priv->hdrv,
                                    &(PIPO_IN_BLOCK){ .sample_data = priv->echo_fifo[ipos], .sample_cnt = priv->echo_buffer_samples, .flags = 0 },
                                    &(uint8_t){1},
                                    channel_to_bitmap(LISA_AUDIO_CH_LEFT));
        if (CSK_DRIVER_OK != ret){
            LOGE("DAC_Echo_Receive_PiPo:%d", ret);
        }
    }
#endif

    portYIELD_FROM_ISR(yield);
}

int arcs_audio_play_config(lisa_audio_play_priv_t *priv, const lisa_audio_play_config_t *config)
{
    int ret = 0;

    if (!config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    memcpy(&priv->config, config, sizeof(lisa_audio_play_config_t));

#ifdef CONFIG_LISA_AUDIO_PLAY_PA_ENABLE
    /* 确保PA处于关闭状态 */
    play_pa_control(false);
#endif

    /* 注意: 即使不启用Echo,也必须设置echo通道,否则会导致杂音 */
    DAC_DMA_CHS dmach = {
        .dma_ch_out_left = GPDMA_DAC0_CHN,
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
        .dma_ch_echo_left = GPDMA_ECHO_CHN,
#endif
    };

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

    uint32_t gain_a = DAC_GAIN_A_VAL(config->gain.analog_gain);
    uint32_t gain_d = DAC_GAIN_D_VAL(config->gain.digital_gain);
    uint32_t vol_flag = 0;
    
    if (hw_channels & LISA_AUDIO_CH_LEFT) {
        vol_flag |= DAC_VOL_FLAG_A_LEFT | DAC_VOL_FLAG_D_LEFT;
    }
    /* 如果硬件只有左声道，忽略右声道增益配置 */

    ret = DAC_SetVolume(priv->hdrv, gain_a, gain_d, vol_flag);
    if (ret != 0) {
        LOGE("DAC_SetVolume failed: %d", ret);
        goto exit;
    }

    /* 计算缓冲区参数 (强制单声道输出) */
    priv->buffer_count = config->buffer_count;
    priv->buffer_samples = config->buffer_samples * MONO_CHANNELS;
    priv->buffer_size = priv->buffer_samples * (uint8_t)config->format.sample_bits;

    if (priv->play_queue) vQueueDelete(priv->play_queue);
    if (priv->free_queue) vQueueDelete(priv->free_queue);
    if (priv->event) vEventGroupDelete(priv->event);
    if (priv->buffer_pool) lisa_mem_free(priv->buffer_pool);
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
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
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    priv->echo_fifo = NULL;
#endif

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

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
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

    priv->ping_addr = NULL;
    priv->pong_addr = NULL;
    priv->state = PLAY_STATE_IDLE;
    priv->status = LISA_AUDIO_STATUS_IDLE;

    LOGI("Play configured: rate=%d, gain=%d/%d dB, buffers=%d×%d",
         config->format.sample_rate,
         config->gain.analog_gain,
         config->gain.digital_gain,
         config->buffer_count,
         config->buffer_samples);

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

int arcs_audio_play_write(lisa_audio_play_priv_t *priv, const void *buffer, uint32_t samples)
{
    if (!buffer || samples == 0) {
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

int arcs_audio_play_get_buffer(lisa_audio_play_priv_t *priv, void **buffer, uint32_t timeout_ms)
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


int arcs_audio_play_control(lisa_audio_play_priv_t *priv, uint32_t cmd, void *arg)
{
    int ret = 0;

    switch (cmd) {
    case LISA_AUDIO_IOCTL_PLAY_START:
        if (priv->state == PLAY_STATE_IDLE) {
            priv->state = PLAY_STATE_PLAY_REQ;

            uint32_t dev_bitmap = channel_to_bitmap(LISA_AUDIO_CH_LEFT);
            play_item_t item1 = {0}, item2 = {0};

            if (xQueueReceive(priv->play_queue, &item1, 0) != pdPASS) {
                if (xQueueReceive(priv->free_queue, &item1, 0) == pdPASS) {
                    memset(item1.addr, 0, priv->buffer_size);
                }
            }

            if (item1.addr) {
                if (xQueueReceive(priv->play_queue, &item2, 0) != pdPASS) {
                    if (xQueueReceive(priv->free_queue, &item2, 0) == pdPASS) {
                        memset(item2.addr, 0, priv->buffer_size);
                    }
                }
            }

            if (item1.addr && item2.addr) {
                HAL_FlushDCache_by_Addr(item1.addr, priv->buffer_size);
                HAL_FlushDCache_by_Addr(item2.addr, priv->buffer_size);

                PIPO_OUT_BLOCK pipo[] = {
                    [0] = { .sample_data = item1.addr, .sample_cnt = priv->buffer_samples, .flags = 0 },
                    [1] = { .sample_data = item2.addr, .sample_cnt = priv->buffer_samples, .flags = 0 },
                };
                priv->ping_addr = item1.addr;
                priv->pong_addr = item2.addr;

                ret = DAC_Send_PiPo(priv->hdrv, pipo, &(uint8_t){2}, dev_bitmap, DAC_TX_FLAG_START_NOW);
                if (ret == 0) {
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
                    priv->echo_xpos = 0;
                    ret = DAC_Echo_Receive_PiPo(priv->hdrv, (PIPO_IN_BLOCK[]){
                        { .sample_data = priv->echo_fifo[0], .sample_cnt = priv->echo_buffer_samples, .flags = 0 },
                        { .sample_data = priv->echo_fifo[1], .sample_cnt = priv->echo_buffer_samples, .flags = 0 },
                    }, &(uint8_t){2}, dev_bitmap);
                    if (ret != 0) {
                        LOGE("DAC_Echo_Receive_PiPo failed: %d", ret);
                    }
#endif
                    DAC_SetMute(priv->hdrv, 0, dev_bitmap);
                    priv->status = LISA_AUDIO_STATUS_RUNNING;
                    LOGI("Play started");
                } else {
                    LOGE("DAC_Send_PiPo failed: %d", ret);
                    priv->state = PLAY_STATE_IDLE;
                    xQueueSendToBack(priv->free_queue, &item1, 0);
                    xQueueSendToBack(priv->free_queue, &item2, 0);
                }
            } else {
                LOGE("Failed to get start buffers");
                priv->state = PLAY_STATE_IDLE;
                if (item1.addr) xQueueSendToBack(priv->free_queue, &item1, 0);
            }
        }

        break;

    case LISA_AUDIO_IOCTL_PLAY_STOP:
        if (priv->state != PLAY_STATE_IDLE) {
            uint32_t dev_bitmap = channel_to_bitmap(LISA_AUDIO_CH_LEFT);

            DAC_SetMute(priv->hdrv, dev_bitmap, dev_bitmap);
#ifdef CONFIG_LISA_AUDIO_PLAY_PA_ENABLE
            play_pa_control(false);
#endif
            DAC_Abort(priv->hdrv, dev_bitmap, dev_bitmap);
            priv->state = PLAY_STATE_IDLE;

            play_item_t item;
            while (xQueueReceive(priv->play_queue, &item, 0) == pdPASS) {
                xQueueSendToBack(priv->free_queue, &item, 0);
            }

            if (priv->ping_addr) {
                item.addr = priv->ping_addr;
                item.size = 0;
                xQueueSendToBack(priv->free_queue, &item, 0);
                priv->ping_addr = NULL;
            }
            if (priv->pong_addr) {
                item.addr = priv->pong_addr;
                item.size = 0;
                xQueueSendToBack(priv->free_queue, &item, 0);
                priv->pong_addr = NULL;
            }

            priv->status = LISA_AUDIO_STATUS_IDLE;
        }

        break;

    case LISA_AUDIO_IOCTL_PLAY_SET_GAIN:
        if (arg) {
            lisa_audio_gain_t *gain = (lisa_audio_gain_t *)arg;
            uint32_t gain_a = DAC_GAIN_A_VAL(gain->analog_gain);
            uint32_t gain_d = DAC_GAIN_D_VAL(gain->digital_gain);
            uint32_t vol_flag = DAC_VOL_FLAG_A_LEFT | DAC_VOL_FLAG_D_LEFT;

            ret = DAC_SetVolume(priv->hdrv, gain_a, gain_d, vol_flag);
            LOGI("Play gain set: %d/%d dB", gain->analog_gain, gain->digital_gain);
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

int arcs_audio_play_init(lisa_audio_play_priv_t *priv)
{
    memset(priv, 0, sizeof(lisa_audio_play_priv_t));

    priv->hdrv = DAC01();
    priv->status = LISA_AUDIO_STATUS_IDLE;
    priv->state = PLAY_STATE_IDLE;

    priv->initialized = true;

    LOGI("LISA Audio Play initialized");

    return LISA_DEVICE_OK;
}
