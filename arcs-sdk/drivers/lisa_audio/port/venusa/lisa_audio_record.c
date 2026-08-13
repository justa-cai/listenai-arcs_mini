/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "lisa_audio_record"

#include <string.h>
#include "lisa_audio_internal.h"
#include "audio_adc_init.h"
#include "lisa_log.h"
#include "sysheap.h"
#include "Driver_ADC_PDM.h"
#include "Driver_Common.h"
#include "cache.h"
#include "systick.h"

/* Must be defined in lisa_audio_venusa.c */
extern int audio_submit_event_from_isr(internal_audio_event_t *event);

/* DMA 通道定义 (通过 Kconfig 配置) */
#define GPDMA_ADC0_CHN  CONFIG_LISA_AUDIO_RECORD_DMA_CHN_LEFT
#define GPDMA_ADC1_CHN  CONFIG_LISA_AUDIO_RECORD_DMA_CHN_RIGHT

/* 前向声明 */
static void record_event_callback(uint32_t event, uint32_t user);

#define RECORD_SAMPLE_BITS 16

/* ===== 辅助函数 ===== */

static inline uint32_t channel_to_bitmap(lisa_audio_channel_t channels)
{
    return (uint32_t)channels;
}

static inline uint8_t channel_count(lisa_audio_channel_t channels)
{
    if (channels == LISA_AUDIO_CH_STEREO) return 2;
    return 1;
}

/* ADC 采样率转换（仅支持 8K/16K/48K） */
static uint32_t record_sample_rate_to_ctrl(lisa_audio_rate_t rate)
{
    switch (rate) {
    case LISA_AUDIO_RATE_8K:  return CSK_ADCPDM_SR_8KHZ;
    case LISA_AUDIO_RATE_16K: return CSK_ADCPDM_SR_16KHZ;
    case LISA_AUDIO_RATE_48K: return CSK_ADCPDM_SR_48KHZ;
    /* 不支持的采样率映射到 16K */
    default:
        LOGW("Record unsupported rate %d, fallback to 16K", rate);
        return CSK_ADCPDM_SR_16KHZ;
    }
}

static uint32_t record_get_osr_for_rate(lisa_audio_rate_t rate)
{
    /* SR * OSR 必须是 4M 或 12M */
    switch (rate) {
    case LISA_AUDIO_RATE_8K:
        /* 8000 * 500 = 4M */
        return CSK_ADCPDM_OSR_500;
    case LISA_AUDIO_RATE_16K:
    case LISA_AUDIO_RATE_24K:
        /* 16000 * 250 = 4M, 24000 * 250 = 6M */
        return CSK_ADCPDM_OSR_250;
    case LISA_AUDIO_RATE_48K:
        /* 48000 * 250 = 12M */
        return CSK_ADCPDM_OSR_250;
    default:
        /* 默认使用 OSR_250 */
        return CSK_ADCPDM_OSR_250;
    }
}

static int record_clamp_gain_db(int gain_db, int min_db, int max_db)
{
    if (gain_db < min_db) {
        return min_db;
    }
    if (gain_db > max_db) {
        return max_db;
    }
    return gain_db;
}

static uint32_t record_analog_gain_val(int8_t gain_db)
{
    return ADC_PDM_GAIN_A_VAL(record_clamp_gain_db(gain_db,
                                                   ADC_PDM_GAIN_A_MIN_DB,
                                                   ADC_PDM_GAIN_A_MAX_DB));
}

static uint32_t record_digital_gain_val(int8_t gain_db)
{
    return ADC_PDM_GAIN_D_VAL(record_clamp_gain_db(gain_db,
                                                   ADC_PDM_GAIN_D_MIN_DB,
                                                   ADC_PDM_GAIN_D_MAX_DB));
}

static int record_apply_uniform_gain(lisa_audio_record_priv_t *priv, const lisa_audio_gain_t *gain)
{
    uint32_t gain_a = 0;
    uint32_t gain_d = 0;
    uint32_t vol_flag = 0;
    lisa_audio_channel_t channels = priv->config.format.channels;

    if (channels & LISA_AUDIO_CH_LEFT) {
        gain_a |= record_analog_gain_val(gain->analog_gain);
        gain_d |= record_digital_gain_val(gain->digital_gain);
        vol_flag |= ADC_PDM_VOL_FLAG_A_LEFT | ADC_PDM_VOL_FLAG_D_LEFT;
    }
    if (channels & LISA_AUDIO_CH_RIGHT) {
        gain_a |= record_analog_gain_val(gain->analog_gain) << 16;
        gain_d |= record_digital_gain_val(gain->digital_gain) << 16;
        vol_flag |= ADC_PDM_VOL_FLAG_A_RIGHT | ADC_PDM_VOL_FLAG_D_RIGHT;
    }

    return ADC_PDM_SetVolume(priv->hdrv, gain_a, gain_d, vol_flag);
}

static int record_apply_channel_gain(lisa_audio_record_priv_t *priv,
                                     const lisa_audio_record_channel_gain_t *gain)
{
    uint32_t gain_a = record_analog_gain_val(gain->left.analog_gain) |
                      (record_analog_gain_val(gain->right.analog_gain) << 16);
    uint32_t gain_d = record_digital_gain_val(gain->left.digital_gain) |
                      (record_digital_gain_val(gain->right.digital_gain) << 16);
    uint32_t vol_flag = ADC_PDM_VOL_FLAG_A_LEFT | ADC_PDM_VOL_FLAG_A_RIGHT |
                        ADC_PDM_VOL_FLAG_D_LEFT | ADC_PDM_VOL_FLAG_D_RIGHT;

    return ADC_PDM_SetVolume(priv->hdrv, gain_a, gain_d, vol_flag);
}

#ifndef DMA_CHANNEL_ANY
#define DMA_CHANNEL_ANY (0xFF)
#endif

static void record_free_buffers(lisa_audio_record_priv_t *priv)
{
    if (!priv->buffers) {
        return;
    }

    for (int i = 0; i < priv->buffer_count; i++) {
        if (priv->buffers[i]) {
            inram_free(priv->buffers[i]);
            priv->buffers[i] = NULL;
        }
    }

    inram_free(priv->buffers);
    priv->buffers = NULL;
}

static int record_alloc_buffers(lisa_audio_record_priv_t *priv)
{
    priv->buffers = inram_malloc(4, sizeof(void *) * priv->buffer_count);
    if (!priv->buffers) {
        LOGE("Failed to allocate record buffer array in SRAM, size=%u",
             (unsigned)(sizeof(void *) * priv->buffer_count));
        return LISA_DEVICE_ERR_NO_MEM;
    }

    memset(priv->buffers, 0, sizeof(void *) * priv->buffer_count);
    for (int i = 0; i < priv->buffer_count; i++) {
        priv->buffers[i] = inram_malloc(32, priv->buffer_size);
        if (!priv->buffers[i]) {
            LOGE("Failed to allocate record DMA buffer %d in SRAM, size=%u",
                 i, (unsigned)priv->buffer_size);
            record_free_buffers(priv);
            return LISA_DEVICE_ERR_NO_MEM;
        }
    }

    return LISA_DEVICE_OK;
}

static void record_submit_completed_block(lisa_audio_record_priv_t *priv,
                                          void *completed_buf,
                                          uint32_t completed_samples)
{
    if (!priv->is_running || completed_buf == NULL || completed_samples == 0) {
        return;
    }

    uint32_t completed_size = completed_samples * (RECORD_SAMPLE_BITS / 8);
    dcache_invalidate_range((uint32_t)completed_buf,
                            (uint32_t)completed_buf + completed_size);

    internal_audio_event_t new_event = {
        .type = AUDIO_EVENT_TYPE_RECORD,
        .buffer = completed_buf,
        .samples = completed_samples,
        .timestamp = SysTimeMsGet() * 1000000ULL,
    };

    if (audio_submit_event_from_isr(&new_event) != 0) {
        LOGW("Record queue full, frame dropped");
    }
}

static void record_event_callback(uint32_t event, uint32_t user)
{
    lisa_audio_record_priv_t *priv = (lisa_audio_record_priv_t *)user;
    uint32_t dev_bitmap = channel_to_bitmap(priv->config.format.channels);

    if (event & CSK_ADCPDM_EVENT_RECEIVE_COMPLETE) {
        void *completed_buf = priv->buffers[priv->current_index];
        record_submit_completed_block(priv, completed_buf, priv->buffer_samples);

        int next_index = priv->current_index;
        if (++next_index >= priv->buffer_count) {
            next_index = 0;
        }

        int32_t ret = ADC_PDM_Receive(priv->hdrv,
                                      priv->buffers[next_index],
                                              priv->buffer_samples,
                                              dev_bitmap,
                                              ADC_PDM_RX_FLAG_START_NOW);
        if (ret == CSK_DRIVER_OK) {
            priv->current_index = next_index;
        } else {
            LOGE("ADC_PDM_Receive refill failed: %d", ret);
        }
    }

    if (event & CSK_ADCPDM_EVENT_RX_FIFO_FULL) {
        LOGW("Record FIFO full");
    }

    if (event & CSK_ADCPDM_EVENT_RX_FIFO_OVERRUN) {
        LOGE("Record FIFO overrun");
    }
}

/* ===== Record API 实现 ===== */

int venusa_audio_record_config(lisa_audio_record_priv_t *priv, const lisa_audio_record_config_t *config)
{
    int ret = 0;

    if (!config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    memcpy(&priv->config, config, sizeof(lisa_audio_record_config_t));

    /* 配置 DMA 通道 */
    ADC_PDM_DMA_CHS dmach = {
        .dma_ch_in_left = DMA_CHANNEL_ANY,
        .dma_ch_in_right = DMA_CHANNEL_ANY
    };

    if (config->format.channels & LISA_AUDIO_CH_LEFT) {
        dmach.dma_ch_in_left = GPDMA_ADC0_CHN;
    }
    if (config->format.channels & LISA_AUDIO_CH_RIGHT) {
        dmach.dma_ch_in_right = GPDMA_ADC1_CHN;
    }

    /* 初始化 ADC 驱动 */
    uint32_t flags = ADC_PDM_BMP_FLAG_USE_16BITS | channel_to_bitmap(config->format.channels);
    #if CONFIG_LISA_AUDIO_RECORD_USE_DMIC
    flags |= ADC_PDM_BMP_FLAG_USE_PDM;
    #endif

    if (priv->adc_initialized) {
        ADC_PDM_Uninitialize(priv->hdrv);
        priv->adc_initialized = false;
    }

    ret = ADC_PDM_Initialize(priv->hdrv,
                             record_event_callback,
                             (uint32_t)priv,
                             flags,
                             &dmach);
    if (ret != 0) {
        LOGE("ADC_PDM_Initialize failed: %d", ret);
        goto exit;
    }

    priv->adc_initialized = true;

    ret = ADC_PDM_PowerControl(priv->hdrv, CSK_POWER_FULL);
    if (ret != 0) {
        LOGE("ADC_PDM_PowerControl failed: %d", ret);
        goto exit;
    }

    /* Venusa HAL demo uses one Control() call for SR/OSR/RXCFG/HPF/PGA.
     * Keeping the same order avoids reconfiguring APC after the first setup. */
    uint32_t sr_ctrl = record_sample_rate_to_ctrl(config->format.sample_rate);
    uint32_t osr_ctrl = record_get_osr_for_rate(config->format.sample_rate);
    uint32_t control = sr_ctrl | osr_ctrl | CSK_ADCPDM_RXCFG_MIXED | CSK_ADCPDM_PGA_INPUT_SET;
    uint32_t control_arg = 0;

    /* HPF 配置 */
    if (config->enable_hpf) {
        control |= CSK_ADCPDM_HPF_SET;
        control_arg |= CSK_ADCPDM_ARG_HPF1_EN | CSK_ADCPDM_ARG_HPF2_EN | CSK_ADCPDM_ARG_HPF2_CUT(3);
    }

    /* PGA 输入模式 */
    if (config->differential_input) {
        control_arg |= CSK_ADCPDM_ARG_LPGA_INPUT_DIFFER | CSK_ADCPDM_ARG_RPGA_INPUT_DIFFER;
    } else {
        control_arg |= CSK_ADCPDM_ARG_LPGA_INPUT_SINGLE | CSK_ADCPDM_ARG_RPGA_INPUT_SINGLE;
    }

    ret = ADC_PDM_Control(priv->hdrv, control, control_arg);
    if (ret != 0) {
        LOGE("ADC_PDM_Control failed: %d", ret);
        goto exit;
    }

    ret = record_apply_uniform_gain(priv, &config->gain);
    if (ret != 0) {
        LOGE("ADC_PDM_SetVolume failed: %d", ret);
        goto exit;
    }

    /* Unmute configured channels */
    uint32_t mute_channels = channel_to_bitmap(config->format.channels);

    ret = ADC_PDM_SetMute(priv->hdrv, 0, mute_channels);
    if (ret != 0) {
        LOGE("ADC_PDM_SetMute failed: %d", ret);
        ret = LISA_DEVICE_ERR_INIT_FAIL;
        goto exit;
    }

    priv->current_index = 0;
    priv->status = LISA_AUDIO_STATUS_IDLE;

exit:
    return ret;
}

static int record_start_locked(lisa_audio_record_priv_t *priv)
{
    int ret = LISA_DEVICE_OK;

    if (!priv->is_running) {
        priv->buffer_count = CONFIG_LISA_AUDIO_RECORD_BUFFER_COUNT;
        priv->buffer_samples = CONFIG_LISA_AUDIO_RECORD_BUFFER_SAMPLES * channel_count(priv->config.format.channels);
        priv->buffer_size = priv->buffer_samples * (RECORD_SAMPLE_BITS / 8);

        if (!priv->buffers) {
            ret = record_alloc_buffers(priv);
            if (ret != LISA_DEVICE_OK) {
                return ret;
            }
        }

        priv->current_index = 0;
        priv->is_running = true;
        priv->status = LISA_AUDIO_STATUS_RUNNING;

        uint32_t dev_bitmap = channel_to_bitmap(priv->config.format.channels);

        ret = ADC_PDM_Receive(priv->hdrv,
                                priv->buffers[0],
                                priv->buffer_samples,
                                dev_bitmap,
                                ADC_PDM_RX_FLAG_START_NOW);

        if (ret != 0) {
            priv->is_running = false;
            priv->status = LISA_AUDIO_STATUS_IDLE;
            LOGE("ADC_PDM_Receive failed: %d", ret);
            /* 错误处理: 如果启动失败，应考虑释放资源，这里暂时保持原有逻辑 */
            return LISA_DEVICE_ERR_IO;
        }
    }

    return ret;
}

static int record_stop_locked(lisa_audio_record_priv_t *priv)
{
    if (priv->is_running) {
        uint32_t dev_bitmap = channel_to_bitmap(priv->config.format.channels);
        ADC_PDM_Abort(priv->hdrv, dev_bitmap);
        priv->is_running = false;
        priv->status = LISA_AUDIO_STATUS_IDLE;
        record_free_buffers(priv);

    }
    return LISA_DEVICE_OK;
}

int venusa_audio_record_control(lisa_audio_record_priv_t *priv, uint32_t cmd, void *arg)
{
    int ret = 0;

    switch (cmd) {
    case LISA_AUDIO_IOCTL_RECORD_START:
        ret = record_start_locked(priv);
        break;

    case LISA_AUDIO_IOCTL_RECORD_STOP:
        ret = record_stop_locked(priv);
        break;

    case LISA_AUDIO_IOCTL_RECORD_PAUSE:
        if (priv->is_running) {
            priv->is_running = false;
            ADC_PDM_Disable(priv->hdrv, channel_to_bitmap(priv->config.format.channels));
            priv->status = LISA_AUDIO_STATUS_PAUSED;
        }
        break;

    case LISA_AUDIO_IOCTL_RECORD_RESUME:
        if (!priv->is_running && priv->status == LISA_AUDIO_STATUS_PAUSED) {
            priv->is_running = true;
            ADC_PDM_Enable(priv->hdrv, channel_to_bitmap(priv->config.format.channels));
            priv->status = LISA_AUDIO_STATUS_RUNNING;
        }
        break;

    case LISA_AUDIO_IOCTL_RECORD_RESET:
        break;

    case LISA_AUDIO_IOCTL_RECORD_SET_GAIN:
        if (arg) {
            lisa_audio_gain_t *gain = (lisa_audio_gain_t *)arg;

            ret = record_apply_uniform_gain(priv, gain);
        }
        break;

    case LISA_AUDIO_IOCTL_RECORD_SET_CHANNEL_GAIN:
        if (arg) {
            lisa_audio_record_channel_gain_t *gain = (lisa_audio_record_channel_gain_t *)arg;

            ret = record_apply_channel_gain(priv, gain);
        } else {
            ret = LISA_DEVICE_ERR_INVALID;
        }
        break;

    case LISA_AUDIO_IOCTL_RECORD_GET_STATUS:
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

int venusa_audio_record_init(lisa_audio_record_priv_t *priv)
{
    memset(priv, 0, sizeof(lisa_audio_record_priv_t));

    audio_adc_platform_init();

    priv->hdrv = ADC_PDM01();
    priv->status = LISA_AUDIO_STATUS_IDLE;
    priv->is_running = false;

    priv->initialized = true;

    return LISA_DEVICE_OK;
}

int venusa_audio_record_deinit(lisa_audio_record_priv_t *priv)
{
    if (priv == NULL) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 停止采集并释放运行期 buffer（record_stop_locked 内部按 is_running 判定，
     * 会 ADC_PDM_Abort 并 record_free_buffers 释放 buffers 数组与各 buffer） */
    record_stop_locked(priv);

    /* 关闭 ADC PDM：先 PowerControl(OFF) 再 Uninitialize，使硬件回到上电初始态 */
    if (priv->hdrv) {
        ADC_PDM_PowerControl(priv->hdrv, CSK_POWER_OFF);
        ADC_PDM_Uninitialize(priv->hdrv);
    }

    priv->status = LISA_AUDIO_STATUS_IDLE;
    priv->is_running = false;
    priv->adc_initialized = false;
    priv->initialized = false;

    return LISA_DEVICE_OK;
}
