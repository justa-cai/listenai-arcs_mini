/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_lisa_audio.c
 * @brief LISA Audio 驱动功能测试
 */

#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_audio.h"
#include "lisa_device.h"

#define AUDIO_DEVICE_NAME        "audio0"

#define SAMPLE_RATE              LISA_AUDIO_RATE_16K
#define SAMPLE_RATE_8K           LISA_AUDIO_RATE_8K
#define SAMPLE_CHANNEL           LISA_AUDIO_CH_LEFT
#define SAMPLE_BITS              LISA_AUDIO_BIT_16

#define RECORD_BUFFER_COUNT      8
#define RECORD_BUFFER_SAMPLES    256

#define PLAY_BUFFER_COUNT        8
#define PLAY_BUFFER_SAMPLES      256
#define PLAY_TEST_SAMPLES        (PLAY_BUFFER_SAMPLES * 4)

static lisa_device_t *g_audio_dev = NULL;
static volatile uint32_t g_record_events = 0;
static volatile uint32_t g_echo_events = 0;
static volatile uint32_t g_paired_events = 0;
static volatile uint32_t g_record_only_events = 0;
static volatile uint32_t g_echo_only_events = 0;
static int16_t g_play_buffer[PLAY_TEST_SAMPLES];

static void audio_test_callback(const lisa_audio_event_t *event, void *user_data)
{
    (void)user_data;

    if (event) {
        bool has_record = (event->record_buffer != NULL);
        bool has_echo = (event->echo_buffer != NULL);

        if (has_record) {
            g_record_events += event->record_samples;
        }
        if (has_echo) {
            g_echo_events += event->echo_samples;
        }

        /* 统计分发模式 */
        if (has_record && has_echo) {
            g_paired_events++;
        } else if (has_record) {
            g_record_only_events++;
        } else if (has_echo) {
            g_echo_only_events++;
        }
    }
}

static lisa_audio_record_config_t make_record_config(int analog_gain, int digital_gain)
{
    lisa_audio_record_config_t cfg = {
        .format = {
            .sample_rate = SAMPLE_RATE,
            .channels = SAMPLE_CHANNEL,
            .sample_bits = SAMPLE_BITS,
        },
        .gain = {
            .analog_gain = analog_gain,
            .digital_gain = digital_gain,
        },
    };
    return cfg;
}

static lisa_audio_play_config_t make_play_config(int analog_gain, int digital_gain,
                                                uint8_t buffer_count, uint16_t buffer_samples)
{
    lisa_audio_play_config_t cfg = {
        .format = {
            .sample_rate = SAMPLE_RATE,
            .channels = SAMPLE_CHANNEL,
            .sample_bits = SAMPLE_BITS,
        },
        .gain = {
            .analog_gain = analog_gain,
            .digital_gain = digital_gain,
        },
        .buffer_count = buffer_count,
        .buffer_samples = buffer_samples,
    };
    return cfg;
}

static void fill_play_buffer(void)
{
    for (uint32_t i = 0; i < PLAY_TEST_SAMPLES; i++) {
        g_play_buffer[i] = (int16_t)(i * 97 % 32767);
    }
}

static int audio_configure_record(void)
{
    lisa_audio_record_config_t config = make_record_config(16, 0);
    config.enable_hpf = true;
    config.differential_input = true;
    return lisa_audio_record_config(g_audio_dev, &config);
}

static int audio_configure_play(void)
{
    lisa_audio_play_config_t config = make_play_config(0, -12, PLAY_BUFFER_COUNT, PLAY_BUFFER_SAMPLES);
    return lisa_audio_play_config(g_audio_dev, &config);
}

/* 8K 采样率配置辅助函数 */
static int audio_configure_record_8k(void)
{
    lisa_audio_record_config_t config = {
        .format = {
            .sample_rate = SAMPLE_RATE_8K,
            .channels = SAMPLE_CHANNEL,
            .sample_bits = SAMPLE_BITS,
        },
        .gain = {
            .analog_gain = 16,
            .digital_gain = 0,
        },
        .enable_hpf = true,
        .differential_input = true,
    };
    return lisa_audio_record_config(g_audio_dev, &config);
}

void setUp(void)
{
    g_record_events = 0;
    g_echo_events = 0;
    g_paired_events = 0;
    g_record_only_events = 0;
    g_echo_only_events = 0;
    memset(g_play_buffer, 0, sizeof(g_play_buffer));
}

void tearDown(void)
{
}

void test_audio_get_device(void)
{
    g_audio_dev = lisa_device_get(AUDIO_DEVICE_NAME);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_audio_dev, "Failed to get audio device");
}

void test_audio_register_callback(void)
{
    TEST_ASSERT_NOT_NULL(g_audio_dev);
    int ret = lisa_audio_register_callback(g_audio_dev, audio_test_callback, NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "register callback failed");
}

void test_audio_record_config_start_stop(void)
{
    TEST_ASSERT_NOT_NULL(g_audio_dev);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, audio_configure_record());

    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_record_start(g_audio_dev));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_record_stop(g_audio_dev));

    TEST_ASSERT_TRUE_MESSAGE(g_record_events > 0, "record callback not triggered");
}

void test_audio_play_config_start_stop(void)
{
    TEST_ASSERT_NOT_NULL(g_audio_dev);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, audio_configure_play());

    fill_play_buffer();

    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_play_start(g_audio_dev));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, lisa_audio_play_write(g_audio_dev, g_play_buffer, PLAY_TEST_SAMPLES));
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_play_flush(g_audio_dev));
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_play_stop(g_audio_dev));
}

void test_audio_record_reconfig(void)
{
    TEST_ASSERT_NOT_NULL(g_audio_dev);

    lisa_audio_record_config_t cfg = make_record_config(10, -2);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_record_config(g_audio_dev, &cfg));

    cfg.gain.analog_gain = 20;
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_record_config(g_audio_dev, &cfg));
}

void test_audio_play_reconfig(void)
{
    TEST_ASSERT_NOT_NULL(g_audio_dev);

    lisa_audio_play_config_t cfg = make_play_config(0, -6, PLAY_BUFFER_COUNT, PLAY_BUFFER_SAMPLES);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_play_config(g_audio_dev, &cfg));

    cfg.gain.digital_gain = -18;
    cfg.buffer_count = PLAY_BUFFER_COUNT + 2;
    cfg.buffer_samples = PLAY_BUFFER_SAMPLES * 2;
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_play_config(g_audio_dev, &cfg));
}

void test_audio_phase_compensation(void)
{
    TEST_ASSERT_NOT_NULL(g_audio_dev);

    lisa_audio_phase_compensation_t set_comp = {
        .record_skip_samples = 4,
        .echo_skip_samples = 2,
    };
    lisa_audio_phase_compensation_t get_comp = {0};

    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_set_phase_compensation(g_audio_dev, &set_comp));
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_get_phase_compensation(g_audio_dev, &get_comp));

    TEST_ASSERT_EQUAL_UINT16(set_comp.record_skip_samples, get_comp.record_skip_samples);
    TEST_ASSERT_EQUAL_UINT16(set_comp.echo_skip_samples, get_comp.echo_skip_samples);
}

void test_audio_invalid_parameters(void)
{
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_ERR_INVALID, lisa_audio_record_config(g_audio_dev, NULL));
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_ERR_INVALID, lisa_audio_play_config(g_audio_dev, NULL));
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_ERR_INVALID, lisa_audio_set_phase_compensation(g_audio_dev, NULL));
}

void test_audio_unregister_callback(void)
{
    TEST_ASSERT_NOT_NULL(g_audio_dev);
    int ret = lisa_audio_unregister_callback(g_audio_dev, audio_test_callback);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "unregister callback failed");
}

/**
 * @brief 测试 8K 采样率录音
 *
 * 验证：8K 采样率应该能正常配置和启动录音
 * 8K 采样率需要使用 OSR_500 (SR * OSR = 4M)
 */
void test_audio_record_8k_sample_rate(void)
{
    TEST_ASSERT_NOT_NULL(g_audio_dev);

    /* 配置 8K 采样率录音 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, audio_configure_record_8k());

    /* 启动录音 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_record_start(g_audio_dev));
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 停止录音 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_record_stop(g_audio_dev));

    /* 验证：应该有录音数据 */
    TEST_ASSERT_TRUE_MESSAGE(g_record_events > 0, "8K record callback not triggered");
}

/* ===== 智能分发功能测试 ===== */

/**
 * @brief 测试只启动录音时的单独分发
 * 
 * 验证：只启动录音时，回调应该只收到录音数据，ECHO为NULL
 */
void test_audio_dispatch_record_only(void)
{
    TEST_ASSERT_NOT_NULL(g_audio_dev);
    
    /* 注册回调 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_register_callback(g_audio_dev, audio_test_callback, NULL));
    
    /* 只配置和启动录音 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, audio_configure_record());
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_record_start(g_audio_dev));
    
    /* 等待录音数据 */
    vTaskDelay(pdMS_TO_TICKS(200));
    
    /* 停止录音 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_record_stop(g_audio_dev));
    
    /* 验证：应该只有录音数据，没有ECHO数据 */
    TEST_ASSERT_GREATER_THAN_UINT32(0, g_record_only_events);
    TEST_ASSERT_EQUAL_UINT32(0, g_echo_only_events);
    TEST_ASSERT_EQUAL_UINT32(0, g_paired_events);
    TEST_ASSERT_GREATER_THAN_UINT32(0, g_record_events);
    TEST_ASSERT_EQUAL_UINT32(0, g_echo_events);
    
    /* 注销回调 */
    lisa_audio_unregister_callback(g_audio_dev, audio_test_callback);
}

/**
 * @brief 测试只启动播放时的单独分发
 * 
 * 验证：只启动播放时，回调应该只收到ECHO数据，录音为NULL
 */
void test_audio_dispatch_play_only(void)
{
    TEST_ASSERT_NOT_NULL(g_audio_dev);
    
    /* 注册回调 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_register_callback(g_audio_dev, audio_test_callback, NULL));
    
    /* 只配置和启动播放 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, audio_configure_play());
    fill_play_buffer();
    
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_play_start(g_audio_dev));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, lisa_audio_play_write(g_audio_dev, g_play_buffer, PLAY_TEST_SAMPLES));
    
    /* 等待播放和ECHO数据 */
    vTaskDelay(pdMS_TO_TICKS(200));
    
    /* 停止播放 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_play_stop(g_audio_dev));
    
    /* 验证：应该只有ECHO数据，没有录音数据 */
    TEST_ASSERT_EQUAL_UINT32(0, g_record_only_events);
    TEST_ASSERT_GREATER_THAN_UINT32(0, g_echo_only_events);
    TEST_ASSERT_EQUAL_UINT32(0, g_paired_events);
    TEST_ASSERT_EQUAL_UINT32(0, g_record_events);
    TEST_ASSERT_GREATER_THAN_UINT32(0, g_echo_events);
    
    /* 注销回调 */
    lisa_audio_unregister_callback(g_audio_dev, audio_test_callback);
}

/**
 * @brief 测试同时启动录音和播放时的严格配对分发
 * 
 * 验证：同时启动时，回调应该收到配对的录音和ECHO数据
 */
void test_audio_dispatch_paired(void)
{
    TEST_ASSERT_NOT_NULL(g_audio_dev);
    
    /* 注册回调 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_register_callback(g_audio_dev, audio_test_callback, NULL));
    
    /* 配置录音和播放 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, audio_configure_record());
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, audio_configure_play());
    fill_play_buffer();
    
    /* 同时启动录音和播放 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_record_start(g_audio_dev));
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_play_start(g_audio_dev));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, lisa_audio_play_write(g_audio_dev, g_play_buffer, PLAY_TEST_SAMPLES));
    
    /* 等待数据 */
    vTaskDelay(pdMS_TO_TICKS(200));
    
    /* 停止录音和播放 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_record_stop(g_audio_dev));
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_play_stop(g_audio_dev));
    
    /* 验证：应该只有配对事件，没有单独事件 */
    TEST_ASSERT_EQUAL_UINT32(0, g_record_only_events);
    TEST_ASSERT_EQUAL_UINT32(0, g_echo_only_events);
    TEST_ASSERT_GREATER_THAN_UINT32(0, g_paired_events);
    TEST_ASSERT_GREATER_THAN_UINT32(0, g_record_events);
    TEST_ASSERT_GREATER_THAN_UINT32(0, g_echo_events);
    
    /* 注销回调 */
    lisa_audio_unregister_callback(g_audio_dev, audio_test_callback);
}

/**
 * @brief 测试先启动录音，后启动播放的场景
 * 
 * 验证：启动播放前应该是单独分发，启动后应该是配对分发
 */
void test_audio_dispatch_sequential_start(void)
{
    TEST_ASSERT_NOT_NULL(g_audio_dev);
    
    /* 注册回调 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_register_callback(g_audio_dev, audio_test_callback, NULL));
    
    /* 配置录音和播放 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, audio_configure_record());
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, audio_configure_play());
    fill_play_buffer();
    
    /* 先启动录音 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_record_start(g_audio_dev));
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* 此时应该有录音单独事件 */
    uint32_t record_only_before = g_record_only_events;
    TEST_ASSERT_GREATER_THAN_UINT32(0, record_only_before);
    TEST_ASSERT_EQUAL_UINT32(0, g_paired_events);
    
    /* 启动播放 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_play_start(g_audio_dev));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, lisa_audio_play_write(g_audio_dev, g_play_buffer, PLAY_TEST_SAMPLES));
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* 现在应该有配对事件 */
    TEST_ASSERT_GREATER_THAN_UINT32(0, g_paired_events);
    
    /* 停止 */
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_record_stop(g_audio_dev));
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, lisa_audio_play_stop(g_audio_dev));
    
    /* 注销回调 */
    lisa_audio_unregister_callback(g_audio_dev, audio_test_callback);
}

int main(void)
{
    UnityBegin("test/driver/lisa_audio/test_lisa_audio.c");

    RUN_TEST(test_audio_get_device, __LINE__);
    RUN_TEST(test_audio_register_callback, __LINE__);
    RUN_TEST(test_audio_record_config_start_stop, __LINE__);
    RUN_TEST(test_audio_record_reconfig, __LINE__);
    RUN_TEST(test_audio_play_config_start_stop, __LINE__);
    RUN_TEST(test_audio_play_reconfig, __LINE__);
    RUN_TEST(test_audio_phase_compensation, __LINE__);
    RUN_TEST(test_audio_invalid_parameters, __LINE__);

    /* 8K 采样率测试 */
    RUN_TEST(test_audio_record_8k_sample_rate, __LINE__);

    /* 智能分发功能测试 */
    RUN_TEST(test_audio_dispatch_record_only, __LINE__);
    RUN_TEST(test_audio_dispatch_play_only, __LINE__);
    RUN_TEST(test_audio_dispatch_paired, __LINE__);
    RUN_TEST(test_audio_dispatch_sequential_start, __LINE__);
    
    RUN_TEST(test_audio_unregister_callback, __LINE__);

    return UnityEnd();
}
