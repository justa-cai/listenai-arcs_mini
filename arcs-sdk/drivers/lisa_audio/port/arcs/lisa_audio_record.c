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
#include "lisa_mem.h"
#include "Driver_ADC_PDM.h"
#include "Driver_Common.h"
#include "dma.h"
#include "cache.h"
#include "systick.h"

/* Must be defined in lisa_audio_arcs.c */
extern int audio_submit_event_from_isr(internal_audio_event_t *event);

/* DMA 通道定义 */
#define GPDMA_ADC0_CHN  (1)
#define GPDMA_ADC1_CHN  (3)

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

/* ===== Record 事件回调 ===== */

#ifndef DMA_CHANNEL_ANY
#define DMA_CHANNEL_ANY (0xFF)
#endif

static void record_event_callback(uint32_t event, uint32_t user)
{
    lisa_audio_record_priv_t *priv = (lisa_audio_record_priv_t *)user;
    int32_t ret;
    int submit_ret = -1;

    if (event & (CSK_ADCPDM_EVENT_RECEIVE_COMPLETE | CSK_ADCPDM_EVENT_BLOCK_COMPLETE)) {
        /* 获取已完成的缓冲区 */
        void *completed_buf = priv->buffers[priv->current_index];

        /* Invalidate cache to ensure CPU reads fresh data from DMA */
        dcache_invalidate_range((uint32_t)completed_buf, (uint32_t)completed_buf + priv->buffer_size);

        /* 先尝试提交事件，只有成功时才更新索引，避免数据错乱 */
        
        if (priv->is_running) {
            internal_audio_event_t new_event = {
                .type = AUDIO_EVENT_TYPE_RECORD,
                .buffer = completed_buf,
                .samples = priv->buffer_samples,
                .timestamp = SysTimeMsGet() * 1000000ULL,
            };
            submit_ret = audio_submit_event_from_isr(&new_event);
        }

        /* 只有在事件成功提交后才更新 current_index */
        if (submit_ret == 0) {
            if (++priv->current_index >= priv->buffer_count) {
                priv->current_index = 0;
            }
        } else if (submit_ret != 0 && priv->is_running) {
            /* 事件提交失败，保持 current_index 不变，下次继续使用同一个 buffer */
            LOGW("Event submit failed, reusing buffer %d", priv->current_index);
        }

        /* 计算下一个用于 DMA 的 buffer 索引 */
        int next_index = priv->current_index;
        if (++next_index >= priv->buffer_count) {
            next_index = 0;
        }

        uint32_t dev_bitmap = channel_to_bitmap(priv->config.format.channels);

        #ifdef CONFIG_LISA_AUDIO_RECORD_USE_PIPO
        /* PiPo 模式 */
        ret = ADC_PDM_Receive_PiPo(priv->hdrv,
                             &(PIPO_IN_BLOCK){
                                 .sample_data = priv->buffers[next_index],
                                 .sample_cnt = priv->buffer_samples,
                                 .flags = 0
                             },
                             &(uint8_t){1},
                             dev_bitmap,
                             ADC_PDM_RX_FLAG_START_NOW);
        #else
        /* 标准模式 */
        ret = ADC_PDM_Receive(priv->hdrv,
                       priv->buffers[next_index],
                       priv->buffer_samples,
                       dev_bitmap,
                       ADC_PDM_RX_FLAG_START_NOW);
        #endif

        if(ret != CSK_DRIVER_OK){
            CLOG("record_event_callback ADC_PDM_Receive ret:%d", ret);
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

int arcs_audio_record_config(lisa_audio_record_priv_t *priv, const lisa_audio_record_config_t *config)
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

    /* 采样率和 OSR */
    uint32_t sr_ctrl = record_sample_rate_to_ctrl(config->format.sample_rate);
    uint32_t osr_ctrl = record_get_osr_for_rate(config->format.sample_rate);
    ret = ADC_PDM_Control(priv->hdrv, sr_ctrl | osr_ctrl, 0);
    if (ret != 0) {
        LOGE("ADC_PDM_Control SR/OSR failed: %d", ret);
        goto exit;
    }

    uint32_t rxcfg = (config->format.channels == LISA_AUDIO_CH_STEREO) ?
                     CSK_ADCPDM_RXCFG_MIXED : CSK_ADCPDM_RXCFG_SEPA;
    ret = ADC_PDM_Control(priv->hdrv, rxcfg, 0);
    if (ret != 0) {
        LOGE("ADC_PDM_Control RXCFG failed: %d", ret);
        goto exit;
    }

    /* HPF 配置 */
    if (config->enable_hpf) {
        ret = ADC_PDM_Control(priv->hdrv,
                              CSK_ADCPDM_HPF_SET,
                              CSK_ADCPDM_ARG_HPF1_EN | CSK_ADCPDM_ARG_HPF2_EN | CSK_ADCPDM_ARG_HPF2_CUT(3));
        if (ret != 0) {
            LOGE("ADC_PDM_Control HPF failed: %d", ret);
            goto exit;
        }
    }

    /* PGA 输入模式 */
    uint32_t pga_mode;
    if (config->differential_input) {
        pga_mode = (CSK_ADCPDM_ARG_LPGA_INPUT_DIFFER | CSK_ADCPDM_ARG_RPGA_INPUT_DIFFER);
    } else {
        pga_mode = (CSK_ADCPDM_ARG_LPGA_INPUT_SINGLE | CSK_ADCPDM_ARG_RPGA_INPUT_SINGLE);
    }

    ret = ADC_PDM_Control(priv->hdrv, CSK_ADCPDM_PGA_INPUT_SET, pga_mode);
    if (ret != 0) {
        LOGE("ADC_PDM_Control PGA failed: %d", ret);
        goto exit;
    }

#ifdef CONFIG_LISA_AUDIO_RECORD_INDIVIDUAL_GAIN
    uint32_t gain_a = ADC_PDM_GAIN_A_VAL(config->gain_l.analog_gain);
    uint32_t gain_d = ADC_PDM_GAIN_D_VAL(config->gain_l.digital_gain);
#else
    uint32_t gain_a = ADC_PDM_GAIN_A_VAL(config->gain.analog_gain);
    uint32_t gain_d = ADC_PDM_GAIN_D_VAL(config->gain.digital_gain);
#endif
    uint32_t vol_flag = 0;

    if (config->format.channels & LISA_AUDIO_CH_LEFT) {
        vol_flag |= ADC_PDM_VOL_FLAG_A_LEFT | ADC_PDM_VOL_FLAG_D_LEFT;
    }
    if (config->format.channels & LISA_AUDIO_CH_RIGHT) {
#ifdef CONFIG_LISA_AUDIO_RECORD_INDIVIDUAL_GAIN
        gain_a |= ADC_PDM_GAIN_A_VAL(config->gain_r.analog_gain) << 16;
        gain_d |= ADC_PDM_GAIN_D_VAL(config->gain_r.digital_gain) << 16;
#else
        gain_a |= ADC_PDM_GAIN_A_VAL(config->gain.analog_gain) << 16;
        gain_d |= ADC_PDM_GAIN_D_VAL(config->gain.digital_gain) << 16;
#endif
        vol_flag |= ADC_PDM_VOL_FLAG_A_RIGHT | ADC_PDM_VOL_FLAG_D_RIGHT;
    }

    ret = ADC_PDM_SetVolume(priv->hdrv, gain_a, gain_d, vol_flag);
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

#ifdef CONFIG_LISA_AUDIO_RECORD_INDIVIDUAL_GAIN
    LOGI("Record configured: rate=%d, gain_l=%d/%d dB, gain_r=%d/%d dB",
         config->format.sample_rate,
         config->gain_l.analog_gain,
         config->gain_l.digital_gain,
         config->gain_r.analog_gain,
         config->gain_r.digital_gain);
#else
    LOGI("Record configured: rate=%d, gain=%d/%d dB",
         config->format.sample_rate,
         config->gain.analog_gain,
         config->gain.digital_gain);
#endif

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
            priv->buffers = lisa_mem_alloc(sizeof(void *) * priv->buffer_count);
            if (!priv->buffers) {
                LOGE("Failed to allocate buffer array, size: %d", sizeof(void *) * priv->buffer_count);
                return LISA_DEVICE_ERR_NO_MEM;
            }

            for (int i = 0; i < priv->buffer_count; i++) {
                priv->buffers[i] = lisa_mem_align_alloc(32, priv->buffer_size);
                if (!priv->buffers[i]) {
                    LOGE("Failed to allocate buffer %d", i);
                    return LISA_DEVICE_ERR_NO_MEM;
                }
            }
        }

        priv->current_index = 0;

        uint32_t dev_bitmap = channel_to_bitmap(priv->config.format.channels);

        #ifdef CONFIG_LISA_AUDIO_RECORD_USE_PIPO
        /* PiPo 双缓冲启动 */
        ret = ADC_PDM_Receive_PiPo(priv->hdrv, (PIPO_IN_BLOCK[]){
            { .sample_data = priv->buffers[0], .sample_cnt = priv->buffer_samples, .flags = 0 },
            { .sample_data = priv->buffers[1], .sample_cnt = priv->buffer_samples, .flags = 0 },
        }, &(uint8_t){2}, dev_bitmap, ADC_PDM_RX_FLAG_START_NOW);
        #else
        /* 标准单缓冲启动 */
        ret = ADC_PDM_Receive(priv->hdrv,
                                priv->buffers[0],
                                priv->buffer_samples,
                                dev_bitmap,
                                ADC_PDM_RX_FLAG_START_NOW);
        #endif

        if (ret == 0) {
            priv->is_running = true;
            priv->status = LISA_AUDIO_STATUS_RUNNING;
            LOGI("Record started");
        } else {
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
        ADC_PDM_Abort(priv->hdrv, channel_to_bitmap(priv->config.format.channels));
        priv->is_running = false;
        priv->status = LISA_AUDIO_STATUS_IDLE;

        if (priv->buffers) {
            for (int i = 0; i < priv->buffer_count; i++) {
                if (priv->buffers[i]) {
                    lisa_mem_free(priv->buffers[i]);
                    priv->buffers[i] = NULL;
                }
            }
            lisa_mem_free(priv->buffers);
            priv->buffers = NULL;
        }

    }
    return LISA_DEVICE_OK;
}

int arcs_audio_record_control(lisa_audio_record_priv_t *priv, uint32_t cmd, void *arg)
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
            LOGI("Record paused");
        }
        break;

    case LISA_AUDIO_IOCTL_RECORD_RESUME:
        if (!priv->is_running && priv->status == LISA_AUDIO_STATUS_PAUSED) {
            priv->is_running = true;
            ADC_PDM_Enable(priv->hdrv, channel_to_bitmap(priv->config.format.channels));
            priv->status = LISA_AUDIO_STATUS_RUNNING;
            LOGI("Record resumed");
        }
        break;

    case LISA_AUDIO_IOCTL_RECORD_RESET:
        LOGI("Record queue reset");
        break;

    case LISA_AUDIO_IOCTL_RECORD_SET_GAIN:
        if (arg) {
            lisa_audio_record_config_t *config = &priv->config;
            lisa_audio_gain_t *gain = (lisa_audio_gain_t *)arg;
#ifdef CONFIG_LISA_AUDIO_RECORD_INDIVIDUAL_GAIN
            uint32_t gain_a = ADC_PDM_GAIN_A_VAL(gain[0].analog_gain);
            uint32_t gain_d = ADC_PDM_GAIN_D_VAL(gain[0].digital_gain);
#else
            uint32_t gain_a = ADC_PDM_GAIN_A_VAL(gain->analog_gain);
            uint32_t gain_d = ADC_PDM_GAIN_D_VAL(gain->digital_gain);
#endif
            uint32_t vol_flag = 0;

            if (config->format.channels & LISA_AUDIO_CH_LEFT) {
                vol_flag |= ADC_PDM_VOL_FLAG_A_LEFT | ADC_PDM_VOL_FLAG_D_LEFT;
            }
            if (config->format.channels & LISA_AUDIO_CH_RIGHT) {
#ifdef CONFIG_LISA_AUDIO_RECORD_INDIVIDUAL_GAIN
                gain_a |= ADC_PDM_GAIN_A_VAL(gain[1].analog_gain) << 16;
                gain_d |= ADC_PDM_GAIN_D_VAL(gain[1].digital_gain) << 16;
#else
                gain_a |= ADC_PDM_GAIN_A_VAL(gain->analog_gain) << 16;
                gain_d |= ADC_PDM_GAIN_D_VAL(gain->digital_gain) << 16;
#endif
                vol_flag |= ADC_PDM_VOL_FLAG_A_RIGHT | ADC_PDM_VOL_FLAG_D_RIGHT;
            }

            ret = ADC_PDM_SetVolume(priv->hdrv, gain_a, gain_d, vol_flag);
#ifdef CONFIG_LISA_AUDIO_RECORD_INDIVIDUAL_GAIN
            LOGI("Record gain set: L=%d/%d dB, R=%d/%d dB",
                 gain[0].analog_gain, gain[0].digital_gain,
                 gain[1].analog_gain, gain[1].digital_gain);
#else
            LOGI("Record gain set: %d/%d dB", gain->analog_gain, gain->digital_gain);
#endif
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

int arcs_audio_record_init(lisa_audio_record_priv_t *priv)
{
    memset(priv, 0, sizeof(lisa_audio_record_priv_t));

    audio_adc_platform_init();

    priv->hdrv = ADC_PDM01();
    priv->status = LISA_AUDIO_STATUS_IDLE;
    priv->is_running = false;

    priv->initialized = true;

    LOGI("LISA Audio Record initialized");

    return LISA_DEVICE_OK;
}
