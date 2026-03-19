/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA Audio 驱动使用示例 - 录音后播音
 *
 * 本示例演示如何使用 LISA Audio 驱动:
 * - 配置 Record 录音参数
 * - 录制 3 秒音频数据
 * - 配置 Play 播音参数
 * - 播放录制的音频
 *
 * 注意: 录音和播音不能同时进行
 */

#include <assert.h>
#define LOG_TAG "audio_sample"
#include <lisa_log.h>

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_device.h"
#include "lisa_audio.h"
#include "audio_clip1.h"
#include "sysheap.h"

#ifdef CONFIG_BOARD_ARCS_MINI
#include "pinmux.h"
#include "Driver_GPIO.h"

/*
 * PA (功放) 使能控制 - ARCS_MINI 使用 PA_EN_PIN (PA1)
 */
static void pa_init(void)
{
    void *gpioa = GPIOA();
    uint32_t pin_mask = (1 << PA_EN_PIN);
    GPIO_Initialize(gpioa, NULL, NULL);
    GPIO_Control(gpioa, CSK_GPIO_DEBOUNCE_DISABLE, pin_mask);
    GPIO_SetDir(gpioa, pin_mask, CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gpioa, pin_mask, 0);
}

static void pa_on(void)
{
    GPIO_PinWrite(GPIOA(), (1 << PA_EN_PIN), 1);
}

static void pa_off(void)
{
    GPIO_PinWrite(GPIOA(), (1 << PA_EN_PIN), 0);
}
#endif /* CONFIG_BOARD_ARCS_MINI */

#define AUDIO_DEVICE_NAME    "audio0"

/* 音频参数配置 */
#define SAMPLE_RATE         LISA_AUDIO_RATE_16K
#define SAMPLE_BITS         LISA_AUDIO_BIT_16
#define CHANNELS            LISA_AUDIO_CH_LEFT

#define BUFFER_COUNT        12
#define BUFFER_SAMPLES      256

/* 录音时长 (秒) */
#define RECORD_DURATION_SEC 3

/* Record 增益配置 */
#ifdef CONFIG_BOARD_ARCS_MINI
#define RECORD_ANALOG_GAIN     36      /* 36 dB */
#define RECORD_DIGITAL_GAIN    0       /* 0 dB */
#else
#define RECORD_ANALOG_GAIN     16      /* 30 dB */
#define RECORD_DIGITAL_GAIN    8       /* 0 dB */
#endif

/* Play 增益配置 */
#define PLAY_ANALOG_GAIN     0       /* 6 dB */
#define PLAY_DIGITAL_GAIN    -12

/* 计算总样本数 */
#define TOTAL_SAMPLES       (16000 * RECORD_DURATION_SEC * CHANNELS)  /* 16kHz × 3秒 × 通道数 */
#define TOTAL_BYTES         (TOTAL_SAMPLES * (uint8_t)SAMPLE_BITS)            /* 16位 = 2字节/样本 */


/* 音频缓冲区 (使用堆分配) */
static int16_t *audio_buffer = NULL;
static uint32_t recorded_samples = 0;
static bool recording_complete = false;

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
/* Echo 缓冲区 */
static int16_t *echo_buffer = NULL;
static uint32_t echo_samples_collected = 0;
static const uint32_t echo_buffer_size = 16000 * 6 * 1;  /* 6秒单声道 Echo 数据 (足够容纳立体声播放的Echo) */
#endif

/**
 * @brief Unified audio callback handler
 */
static void unified_audio_callback(const lisa_audio_event_t *event, void *user_data)
{
    /* Handle Record Data */
    if (event->record_buffer) {
        uint32_t samples_to_record = TOTAL_SAMPLES;
        uint32_t samples = event->record_samples;

        if (recorded_samples < samples_to_record) {
            uint32_t remaining = samples_to_record - recorded_samples;
            uint32_t to_copy = (samples < remaining) ? samples : remaining;
            
            memcpy(&audio_buffer[recorded_samples], event->record_buffer, to_copy * (uint8_t)SAMPLE_BITS);
            recorded_samples += to_copy;
        }
    }

    /* Handle Echo Data */
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    if (event->echo_buffer) {
        uint32_t samples = event->echo_samples;
        if (echo_samples_collected + samples <= echo_buffer_size) {
            memcpy(&echo_buffer[echo_samples_collected], event->echo_buffer, samples * sizeof(int16_t));
            echo_samples_collected += samples;
        }
    }
#endif
}

/**
 * @brief 录音任务
 */
static int record_audio(lisa_device_t *record_dev)
{
    int ret;
    static uint32_t samples_to_record = TOTAL_SAMPLES;

    LISA_LOGI(LOG_TAG, "开始录音 (%d 秒)...", RECORD_DURATION_SEC);

    /* 配置 Record */
    lisa_audio_record_config_t record_config = {
        .format = {
            .sample_rate = SAMPLE_RATE,
            .channels = CHANNELS,
            .sample_bits = SAMPLE_BITS,
        },
        .gain = {
            .analog_gain = RECORD_ANALOG_GAIN,
            .digital_gain = RECORD_DIGITAL_GAIN,
        },
        .differential_input = true,
        .enable_hpf = true,
    };

    ret = lisa_audio_record_config(record_dev, &record_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Record 配置失败: %d", ret);
        return ret;
    }

    /* 重置状态 */
    recorded_samples = 0;
    recording_complete = false;

    /* 启动录音 */
    ret = lisa_audio_record_start(record_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "启动录音失败: %d", ret);
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(RECORD_DURATION_SEC * 1000));

    lisa_audio_record_stop(record_dev);

    LISA_LOGI(LOG_TAG, "录音完成! 共录制 %d 样本", recorded_samples);
    return LISA_DEVICE_OK;
}

/**
 * @brief 播音任务
 */
static int playback_audio(lisa_device_t *play_dev)
{
    int ret;

    if (recorded_samples == 0) {
        LISA_LOGE(LOG_TAG, "没有可播放的音频数据");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "开始播放 (%d 样本)...", recorded_samples);

    /* 配置 Play */
    lisa_audio_play_config_t play_config = {
        .format = {
            .sample_rate = SAMPLE_RATE,
            .channels = CHANNELS,
            .sample_bits = SAMPLE_BITS,
        },
        .gain = {
            .analog_gain = PLAY_ANALOG_GAIN,
            .digital_gain = PLAY_DIGITAL_GAIN,
        },
        .buffer_count = BUFFER_COUNT,
        .buffer_samples = BUFFER_SAMPLES,
    };

    ret = lisa_audio_play_config(play_dev, &play_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Play 配置失败: %d", ret);
        return ret;
    }

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    /* 重置 Echo 计数器 */
    echo_samples_collected = 0;
    LISA_LOGI(LOG_TAG, "开始收集 Echo 数据...");
#endif

    /* 启动播音 */
    ret = lisa_audio_play_start(play_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "启动播音失败: %d", ret);
        return ret;
    }

#ifdef CONFIG_BOARD_ARCS_MINI
    pa_on();
#endif

    lisa_audio_play_write(play_dev, audio_buffer, recorded_samples);

    LOGI("写入音频数据完成");
    /* 等待播放完成 */
    lisa_audio_play_flush(play_dev);

#ifdef CONFIG_BOARD_ARCS_MINI
    pa_off();
#endif

    LOGI("播放完成");
    /* 停止播音 */
    lisa_audio_play_stop(play_dev);

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    LISA_LOGI(LOG_TAG, "Echo 数据收集完成,共收集 %d 样本", echo_samples_collected);
#endif

    LISA_LOGI(LOG_TAG, "播放完成! 共播放 %d 样本", recorded_samples);
    return LISA_DEVICE_OK;
}

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
/**
 * @brief 播放 Echo 数据
 */
static int playback_echo(lisa_device_t *play_dev)
{
    int ret;

    if (echo_samples_collected == 0) {
        LISA_LOGI(LOG_TAG, "没有 Echo 数据可播放");
        return LISA_DEVICE_OK;
    }

    LISA_LOGI(LOG_TAG, "开始播放 Echo 数据 (%d 样本)...", echo_samples_collected);

    /* 等待一段时间确保前一次播放完全停止 */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 重新配置为单声道 (Echo 数据是单声道) */
    lisa_audio_play_config_t play_config = {
        .format = {
            .sample_rate = SAMPLE_RATE,
            .channels = LISA_AUDIO_CH_LEFT,  /* Echo 强制单声道 */
            .sample_bits = SAMPLE_BITS,
        },
        .gain = {
            .analog_gain = PLAY_ANALOG_GAIN,
            .digital_gain = PLAY_DIGITAL_GAIN,
        },
        .buffer_count = BUFFER_COUNT,
        .buffer_samples = BUFFER_SAMPLES,
    };

    ret = lisa_audio_play_config(play_dev, &play_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Echo Play 配置失败: %d", ret);
        return ret;
    }

    /* 启动播音 */
    ret = lisa_audio_play_start(play_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "启动 Echo 播音失败: %d", ret);
        return ret;
    }

#ifdef CONFIG_BOARD_ARCS_MINI
    pa_on();
#endif

    /* 写入 Echo 数据 */
    lisa_audio_play_write(play_dev, echo_buffer, echo_samples_collected);

    /* 等待播放完成 */
    lisa_audio_play_flush(play_dev);

#ifdef CONFIG_BOARD_ARCS_MINI
    pa_off();
#endif

    /* 停止播音 */
    lisa_audio_play_stop(play_dev);

    LISA_LOGI(LOG_TAG, "Echo 播放完成! 共播放 %d 样本", echo_samples_collected);
    return LISA_DEVICE_OK;
}
#endif

int main(int argc, char **argv)
{
    int ret;

    LISA_LOGI(LOG_TAG, "LISA Audio 驱动示例 - 录音后播音");

    /* 获取 Audio 设备 */
    lisa_device_t *audio_dev = lisa_device_get(AUDIO_DEVICE_NAME);
    if (!audio_dev) {
        LISA_LOGE(LOG_TAG, "获取 Audio 设备失败");
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Audio 设备获取成功");

#ifdef CONFIG_BOARD_ARCS_MINI
    pa_init();
#endif

    /* 注册统一回调函数 */
    ret = lisa_audio_register_callback(audio_dev, unified_audio_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "注册统一回调失败: %d", ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "统一回调注册成功");

    /* 分配音频缓冲区 */
    audio_buffer = (int16_t *)exram_malloc(4, TOTAL_BYTES);
    if (!audio_buffer) {
        LISA_LOGE(LOG_TAG, "内存分配失败 (%d 字节)", TOTAL_BYTES);
        lisa_audio_unregister_callback(audio_dev, unified_audio_callback);
        return -1;
    }

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    /* 分配 Echo 缓冲区 */
    echo_buffer = (int16_t *)exram_malloc(4, echo_buffer_size * sizeof(int16_t));
    if (!echo_buffer) {
        LISA_LOGE(LOG_TAG, "Echo 内存分配失败 (%d 字节)", echo_buffer_size * (int)sizeof(int16_t));
        exram_free(audio_buffer);
        lisa_audio_unregister_callback(audio_dev, unified_audio_callback);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Echo 缓冲区分配成功 (%d 样本)", echo_buffer_size);
#endif

    /* 步骤 1: 录音 */
    LISA_LOGI(LOG_TAG, "步骤 1/2: 开始录音...");

    ret = record_audio(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "录音失败!");
        goto cleanup;
    }

    /* 步骤 2: 播音 */
#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    LISA_LOGI(LOG_TAG, "步骤 2/3: 开始播放 (同时收集 Echo)...");
#else
    LISA_LOGI(LOG_TAG, "步骤 2/2: 开始播放...");
#endif
    ret = playback_audio(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "播放失败!");
        goto cleanup;
    }

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    /* 步骤 3: 播放 Echo 数据 */
    LISA_LOGI(LOG_TAG, "步骤 3/3: 开始播放 Echo 数据...");
    ret = playback_echo(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Echo 播放失败!");
        goto cleanup;
    }

    LISA_LOGI(LOG_TAG, "示例完成! 录音、播放和 Echo 播放都成功");
#else
    LISA_LOGI(LOG_TAG, "示例完成! 录音和播放都成功");
#endif

cleanup:
    /* 注销统一回调 */
    lisa_audio_unregister_callback(audio_dev, unified_audio_callback);
    LISA_LOGI(LOG_TAG, "统一回调已注销");

    /* 释放音频缓冲区 */
    if (audio_buffer) {
        exram_free(audio_buffer);
        audio_buffer = NULL;
    }

#ifdef CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE
    /* 释放 Echo 缓冲区 */
    if (echo_buffer) {
        exram_free(echo_buffer);
        echo_buffer = NULL;
    }
#endif

    return ret;
}
