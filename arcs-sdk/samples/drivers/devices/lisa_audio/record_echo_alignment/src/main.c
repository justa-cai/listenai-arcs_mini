/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA Audio 驱动使用示例 - 录音与ECHO相位对齐校准
 *
 * 本示例演示如何使用 LISA Audio 驱动:
 * - 同时进行录音和播音，采集录音数据和ECHO回采数据
 * - 自动测量录音与ECHO之间的系统相位偏移
 * - 验证相位补偿功能的有效性
 * - 通过频率检测和周期归一化确保测量准确性
 */

#include <assert.h>
#include <stdbool.h>
#define LOG_TAG "audio_sample"
#include <lisa_log.h>

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_device.h"
#include "lisa_audio.h"
#include "audio_clip.h"
#include "sysheap.h"
#include <math.h>

#define AUDIO_DEVICE_NAME    "audio0"

/* 音频参数配置 - 单声道(左声道) */
#define SAMPLE_RATE         LISA_AUDIO_RATE_16K
#define SAMPLE_BITS         LISA_AUDIO_BIT_16
#define CHANNELS            LISA_AUDIO_CH_LEFT

#define BUFFER_COUNT        12
#define BUFFER_SAMPLES      256

/* Record 增益配置 */
#define RECORD_ANALOG_GAIN     30      /* 16 dB */
#define RECORD_DIGITAL_GAIN    8       /* 8 dB */

/* Play 增益配置 */
#define PLAY_ANALOG_GAIN     0       /* 6 dB */
#define PLAY_DIGITAL_GAIN    -18

/* 录音缓冲区大小(单声道) - 与播放的 clip 大小一致 */
#define TOTAL_SAMPLES       AUDIO_CLIP_SAMPLES
#define TOTAL_BYTES         (TOTAL_SAMPLES * (uint8_t)SAMPLE_BITS)

/* 录音和 ECHO 缓冲区 */
static int16_t *record_buffer = NULL;
static int16_t *echo_buffer = NULL;
static uint32_t recorded_samples = 0;
static uint32_t echo_samples = 0;

/* 播放源数据(用于测试的音频数据) */
static int16_t *playback_source = NULL;

/* 测试控制 */
static bool test_running = false;

/**
 * @brief Unified audio callback handler - 仅收集数据
 */
static void unified_audio_callback(const lisa_audio_event_t *event, void *user_data)
{
    if (!test_running) {
        return;
    }
    // 现在这两个条件总是同时为真
    assert(event->record_buffer != NULL);
    assert(event->echo_buffer != NULL);

    bool has_record = (event->record_buffer != NULL);
    bool has_echo = (event->echo_buffer != NULL);

    /* 收集录音数据 */
    if (has_record && recorded_samples < TOTAL_SAMPLES) {
        uint32_t to_copy = event->record_samples;
        if (recorded_samples + to_copy > TOTAL_SAMPLES) {
            to_copy = TOTAL_SAMPLES - recorded_samples;
        }
        memcpy(&record_buffer[recorded_samples], event->record_buffer, to_copy * sizeof(int16_t));
        recorded_samples += to_copy;
    }

    /* 收集 ECHO 数据 */
    if (has_echo && echo_samples < TOTAL_SAMPLES) {
        uint32_t to_copy = event->echo_samples;
        if (echo_samples + to_copy > TOTAL_SAMPLES) {
            to_copy = TOTAL_SAMPLES - echo_samples;
        }
        memcpy(&echo_buffer[echo_samples], event->echo_buffer, to_copy * sizeof(int16_t));
        echo_samples += to_copy;
    }
}

/**
 * @brief 使用过零检测法估算信号频率
 * @param signal 信号数据
 * @param len 信号长度
 * @param sample_rate 采样率(Hz)
 * @return 估算的频率(Hz), 失败返回 -1.0
 */
static float estimate_frequency(const int16_t *signal, uint32_t len, uint32_t sample_rate)
{
    if (len < 100) {
        return -1.0f;
    }

    // 计算过零点数量
    uint32_t zero_crossings = 0;
    for (uint32_t i = 1; i < len; i++) {
        if ((signal[i-1] >= 0 && signal[i] < 0) || (signal[i-1] < 0 && signal[i] >= 0)) {
            zero_crossings++;
        }
    }

    // 频率 = (过零点数 / 2) / 持续时间
    float duration = (float)len / (float)sample_rate;
    float frequency = ((float)zero_crossings / 2.0f) / duration;

    return frequency;
}

/**
 * @brief 计算互相关,找到最大相关性的偏移量
 * @param sig1 信号1
 * @param sig2 信号2
 * @param len 信号长度
 * @param max_offset 最大搜索偏移量
 * @return 相位偏移(采样点数),正值表示sig2落后于sig1
 */
static int32_t find_phase_offset(const int16_t *sig1, const int16_t *sig2, uint32_t len, uint32_t max_offset)
{
    int64_t max_corr = INT64_MIN;
    int32_t best_offset = 0;

    // 搜索范围: [-max_offset, +max_offset]
    for (int32_t offset = -(int32_t)max_offset; offset <= (int32_t)max_offset; offset++) {
        int64_t corr = 0;
        uint32_t count = 0;

        // 计算当前偏移下的互相关
        for (uint32_t i = 0; i < len; i++) {
            int32_t idx1 = i;
            int32_t idx2 = i + offset;

            if (idx2 < 0 || idx2 >= (int32_t)len) {
                continue;
            }

            corr += (int64_t)sig1[idx1] * (int64_t)sig2[idx2];
            count++;
        }

        if (count > 0 && corr > max_corr) {
            max_corr = corr;
            best_offset = offset;
        }
    }

    return best_offset;
}

#define PRINT_SAMPLES 200

/**
 * @brief 分析音频相位差 (单声道版本)
 */
static bool analyze_phase_difference(int32_t *out_offset, uint32_t max_search_offset)
{
    uint32_t min_total_samples = (recorded_samples < echo_samples) ? recorded_samples : echo_samples;

    // 跳过开头的 200ms 数据,以过滤启动噪音
    const uint32_t skip_ms = 200;
    const uint32_t skip_samples = (SAMPLE_RATE / 1000) * skip_ms;

    if (min_total_samples <= skip_samples + PRINT_SAMPLES) { // 确保跳过之后还有足够样本进行分析
        LISA_LOGW(LOG_TAG, "采样数不足,无法分析相位 (跳过 %u 样本后)", skip_samples);
        return false;
    }

    const int16_t *effective_record_buffer = &record_buffer[skip_samples];
    const int16_t *effective_echo_buffer = &echo_buffer[skip_samples];
    const uint32_t effective_num_samples = min_total_samples - skip_samples;

    // 步骤1: 频率验证
    float record_freq = estimate_frequency(effective_record_buffer, effective_num_samples, 16000);
    float echo_freq = estimate_frequency(effective_echo_buffer, effective_num_samples, 16000);

    const float freq_tolerance = 10.0f; // 允许 ±10Hz 的误差
    bool record_freq_valid = (fabsf(record_freq - AUDIO_CLIP_FREQ) <= freq_tolerance);
    bool echo_freq_valid = (fabsf(echo_freq - AUDIO_CLIP_FREQ) <= freq_tolerance);

    LISA_LOGI(LOG_TAG, "========================================");
    LISA_LOGI(LOG_TAG, "频率检测结果:");
    LISA_LOGI(LOG_TAG, "  目标频率: %.1f Hz", AUDIO_CLIP_FREQ);
    LISA_LOGI(LOG_TAG, "  录音频率: %.1f Hz %s", record_freq, record_freq_valid ? "[OK]" : "[FAIL]");
    LISA_LOGI(LOG_TAG, "  ECHO频率: %.1f Hz %s", echo_freq, echo_freq_valid ? "[OK]" : "[FAIL]");

    if (!record_freq_valid || !echo_freq_valid) {
        LISA_LOGE(LOG_TAG, "频率验证失败! 跳过相位偏移计算");
        LISA_LOGI(LOG_TAG, "========================================");
        return false;
    }
    LISA_LOGI(LOG_TAG, "频率验证通过, 继续相位偏移计算");
    LISA_LOGI(LOG_TAG, "========================================");

    // 步骤2: 相位偏移计算
    int32_t offset = find_phase_offset(effective_record_buffer, effective_echo_buffer, effective_num_samples, max_search_offset);

    // 步骤3: 周期归一化 - 将偏移归一化到 [-period/2, +period/2] 范围内
    // 周期 = 采样率 / 频率
    float period_samples = 16000.0f / AUDIO_CLIP_FREQ;
    int32_t period = (int32_t)(period_samples + 0.5f); // 四舍五入
    int32_t half_period = period / 2;

    int32_t raw_offset = offset; // 保存原始偏移用于调试

    // 将偏移归一化到 [-half_period, +half_period] 范围
    while (offset > half_period) {
        offset -= period;
    }
    while (offset < -half_period) {
        offset += period;
    }

    if (raw_offset != offset) {
        LISA_LOGI(LOG_TAG, "周期归一化: %d -> %d (周期=%d)", raw_offset, offset, period);
    }

    // 计算时间偏移(微秒)
    float time_offset_us = (float)offset * 1000000.0f / 16000.0f;

    LISA_LOGI(LOG_TAG, "========================================");
    LISA_LOGI(LOG_TAG, "相位分析结果:");
    LISA_LOGI(LOG_TAG, "  录音采样数: %u", recorded_samples);
    LISA_LOGI(LOG_TAG, "  ECHO采样数: %u", echo_samples);
    LISA_LOGI(LOG_TAG, "  相位偏移: %d 个采样点", offset);
    LISA_LOGI(LOG_TAG, "  时间偏移: %.1f us", time_offset_us);

    if (offset > 0) {
        LISA_LOGI(LOG_TAG, "  结论: ECHO 落后于录音");
    } else if (offset < 0) {
        LISA_LOGI(LOG_TAG, "  结论: 录音落后于 ECHO");
    } else {
        LISA_LOGI(LOG_TAG, "  结论: 录音与 ECHO 相位对齐");
    }
    LISA_LOGI(LOG_TAG, "========================================");

    *out_offset = offset;
    return true;
}


/**
 * @brief 同时进行录音和播放测试
 */
static int test_simultaneous_record_play(lisa_device_t *audio_dev, uint16_t record_skip, uint16_t echo_skip, int32_t *out_offset, uint32_t max_search_offset)
{
    int ret;

    LISA_LOGI(LOG_TAG, "========================================");
    LISA_LOGI(LOG_TAG, "开始测试: 录音跳过%u, ECHO跳过%u, 最大搜索偏移%u", record_skip, echo_skip, max_search_offset);
    LISA_LOGI(LOG_TAG, "========================================");

    /* 设置相位补偿 */
    lisa_audio_phase_compensation_t phase_comp = {
        .record_skip_samples = record_skip,
        .echo_skip_samples = echo_skip,
    };
    ret = lisa_audio_set_phase_compensation(audio_dev, &phase_comp);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "设置相位补偿失败: %d", ret);
        return ret;
    }
    LISA_LOGI(LOG_TAG, "相位补偿已设置");

    /* 重置数据收集 */
    recorded_samples = 0;
    echo_samples = 0;
    memset(record_buffer, 0, TOTAL_BYTES);
    memset(echo_buffer, 0, TOTAL_BYTES);

    /* 开始收集数据 */
    test_running = true;

    /* 写入测试音频数据 (单声道) */
    LISA_LOGI(LOG_TAG, "开始写入测试音频...");
    lisa_audio_play_write(audio_dev, playback_source, AUDIO_CLIP_SAMPLES);

    /* 等待播放完成 */
    LISA_LOGI(LOG_TAG, "等待播放完成...");
    lisa_audio_play_flush(audio_dev);

    /* 停止收集数据 */
    test_running = false;

    /* 分析相位差 */
    if (!analyze_phase_difference(out_offset, max_search_offset)) {
        LISA_LOGE(LOG_TAG, "相位分析失败");
        return -1;
    }

    return LISA_DEVICE_OK;
}


int main(int argc, char **argv)
{
    int ret;

    LISA_LOGI(LOG_TAG, "LISA Audio 驱动示例 - 录音与ECHO相位对齐校准");

    /* 获取 Audio 设备 */
    lisa_device_t *audio_dev = lisa_device_get(AUDIO_DEVICE_NAME);
    if (!audio_dev) {
        LISA_LOGE(LOG_TAG, "获取 Audio 设备失败");
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Audio 设备获取成功");

    /* 注册统一回调函数 */
    ret = lisa_audio_register_callback(audio_dev, unified_audio_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "注册统一回调失败: %d", ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "统一回调注册成功");

    /* 分配缓冲区 */
    record_buffer = (int16_t *)exram_malloc(4, TOTAL_BYTES);
    if (!record_buffer) {
        LISA_LOGE(LOG_TAG, "录音缓冲区分配失败 (%d 字节)", TOTAL_BYTES);
        goto cleanup;
    }
    echo_buffer = (int16_t *)exram_malloc(4, TOTAL_BYTES);
    if (!echo_buffer) {
        LISA_LOGE(LOG_TAG, "Echo 缓冲区分配失败 (%d 字节)", TOTAL_BYTES);
        goto cleanup;
    }

    /* 准备测试音频 */
    LISA_LOGI(LOG_TAG, "准备测试音频数据 (使用 audio_clip)...");
    playback_source = (int16_t *)g_audio_clip;
    LISA_LOGI(LOG_TAG, "测试音频数据准备完成 (%.1fHz)", AUDIO_CLIP_FREQ);

    /* 统一配置 Record */
    lisa_audio_record_config_t record_config = {
        .format = { .sample_rate = SAMPLE_RATE, .channels = CHANNELS, .sample_bits = SAMPLE_BITS },
        .gain = { .analog_gain = RECORD_ANALOG_GAIN, .digital_gain = RECORD_DIGITAL_GAIN },
        .differential_input = true,
        .enable_hpf = false,
    };
    ret = lisa_audio_record_config(audio_dev, &record_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Record 配置失败: %d", ret);
        goto cleanup;
    }

    /* 统一配置 Play */
    lisa_audio_play_config_t play_config = {
        .format = { .sample_rate = SAMPLE_RATE, .channels = CHANNELS, .sample_bits = SAMPLE_BITS },
        .gain = { .analog_gain = PLAY_ANALOG_GAIN, .digital_gain = PLAY_DIGITAL_GAIN },
        .buffer_count = BUFFER_COUNT,
        .buffer_samples = BUFFER_SAMPLES,
    };
    ret = lisa_audio_play_config(audio_dev, &play_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Play 配置失败: %d", ret);
        goto cleanup;
    }

    /* 统一启动录音和播音 */
    ret = lisa_audio_record_start(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "启动录音失败: %d", ret);
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "录音已启动");

    ret = lisa_audio_play_start(audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "启动播音失败: %d", ret);
        lisa_audio_record_stop(audio_dev);
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "播音已启动");

    /* 等待音频流稳定 */
    vTaskDelay(pdMS_TO_TICKS(100));

    int32_t offset1 = 0, offset2 = 0;

    // 根据测试音频频率动态计算最大搜索偏移
    // 理论上只需要搜索半个周期，但为了容错，使用 1 个完整周期
    const uint32_t period_samples = (uint32_t)(16000.0f / AUDIO_CLIP_FREQ + 0.5f);
    const uint32_t calibration_max_offset = period_samples; // 1个周期

    LISA_LOGI(LOG_TAG, "测试参数: 频率=%.1fHz, 周期=%u采样点, 最大搜索偏移=%u",
              AUDIO_CLIP_FREQ, period_samples, calibration_max_offset);

    /* --- 测试 1: 测量系统偏移 (第1次) --- */
    LISA_LOGI(LOG_TAG, "\n\n======== 测试 1: 测量系统偏移 (第1次) ========");
    ret = test_simultaneous_record_play(audio_dev, 0, 0, &offset1, calibration_max_offset);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "测试 1 失败!");
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "第1次测量偏移: %d 个采样点", offset1);
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* --- 测试 2: 测量系统偏移 (第2次, 用于验证一致性) --- */
    LISA_LOGI(LOG_TAG, "\n\n======== 测试 2: 测量系统偏移 (第2次, 验证一致性) ========");
    ret = test_simultaneous_record_play(audio_dev, 0, 0, &offset2, calibration_max_offset);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "测试 2 失败!");
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "第2次测量偏移: %d 个采样点", offset2);
    
    if (offset1 == offset2) {
        LISA_LOGI(LOG_TAG, "两次测量结果一致, 系统偏移稳定.");
    } else {
        LISA_LOGW(LOG_TAG, "两次测量结果不一致! (%d vs %d), 系统偏移不稳定, 后续测试可能失败.", offset1, offset2);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));


    /* --- 测试 3: 自动对齐验证 --- */
    int32_t system_offset = offset1; // 使用第一次的测量结果进行补偿
    LISA_LOGI(LOG_TAG, "使用第1次测量结果 (%d) 进行补偿", system_offset);

    uint16_t record_comp = 0;
    uint16_t echo_comp = 0;
    if (system_offset > 0) { // ECHO 落后于录音, 需要跳过 ECHO 前面部分让其追上
        echo_comp = system_offset;
    } else { // 录音落后于 ECHO, 需要跳过录音前面部分让其追上
        record_comp = -system_offset;
    }
    
    const uint32_t verification_max_offset = calibration_max_offset;
    int32_t final_offset = 0;
    LISA_LOGI(LOG_TAG, "\n\n======== 测试 3: 自动对齐验证 (补偿: rec_skip=%u, echo_skip=%u) ========", record_comp, echo_comp);
    ret = test_simultaneous_record_play(audio_dev, record_comp, echo_comp, &final_offset, verification_max_offset);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "测试 3 失败!");
        goto cleanup;
    }

    if (abs(final_offset) <= 1) {
        LISA_LOGI(LOG_TAG, "自动对齐成功! 补偿后测量偏移: %d", final_offset);
    } else {
        LISA_LOGE(LOG_TAG, "自动对齐失败! 补偿后测量偏移: %d", final_offset);
    }

cleanup:
    if (audio_dev) {
        lisa_audio_record_stop(audio_dev);
        lisa_audio_play_stop(audio_dev);
        vTaskDelay(pdMS_TO_TICKS(200)); // 等待资源释放
    }

    LISA_LOGI(LOG_TAG, "\n\n========================================");
    LISA_LOGI(LOG_TAG, "所有测试完成!");
    LISA_LOGI(LOG_TAG, "========================================");

    /* 注销统一回调 */
    if (audio_dev) {
        lisa_audio_unregister_callback(audio_dev, unified_audio_callback);
        LISA_LOGI(LOG_TAG, "统一回调已注销");
    }

    /* 释放缓冲区 */
    if (record_buffer) {
        exram_free(record_buffer);
        record_buffer = NULL;
    }
    if (echo_buffer) {
        exram_free(echo_buffer);
        echo_buffer = NULL;
    }

    return ret;
}