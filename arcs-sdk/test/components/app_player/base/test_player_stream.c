/*
 * LISA App Player Component - 流式接口测试
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * 测试播放器流式播放接口
 */

#include "unity.h"
#include "app_player.h"
#include "test_common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
/* ========================================
 * 测试辅助变量
 * ======================================== */

static int stream_callback_count = 0;
static app_player_event_t stream_last_event = APP_PLAYER_EVENT_ERROR;

/* 静态全局PCM缓冲区 - 包含1kHz正弦波样本数据 (16kHz采样率, 16-bit单声道) */
/* 增加缓冲区大小以产生更长的可听声音：16KB = 0.5秒@16kHz */
static uint8_t g_pcm_buffer[16384];
static int g_pcm_buffer_initialized = 0;

/* ========================================
 * 测试回调函数
 * ======================================== */

static void stream_event_callback(app_player_t *player, app_player_event_t event, void *user_data)
{
    stream_callback_count++;
    stream_last_event = event;
    printf("  [Stream Callback] Event: %s, Count: %d\n",
           test_event_to_string(event), stream_callback_count);
}

static void reset_stream_callback_state(void)
{
    stream_callback_count = 0;
    stream_last_event = APP_PLAYER_EVENT_ERROR;
}

/**
 * @brief 初始化全局PCM缓冲区为1kHz正弦波 (16kHz采样率, 16-bit)
 */
static void init_pcm_buffer(void)
{
    if (g_pcm_buffer_initialized) {
        return;
    }

    int16_t *samples = (int16_t *)g_pcm_buffer;
    int num_samples = sizeof(g_pcm_buffer) / sizeof(int16_t);

    /* 生成1kHz正弦波: f=1000Hz, fs=16000Hz, 幅度为INT16_MAX的70% */
    const double amplitude = 32767.0 * 0.7;
    const double frequency = 1000.0;
    const double sample_rate = 16000.0;
    const double pi = 3.14159265358979323846;

    for (int i = 0; i < num_samples; i++) {
        double t = (double)i / sample_rate;
        samples[i] = (int16_t)(amplitude * sin(2.0 * pi * frequency * t));
    }

    g_pcm_buffer_initialized = 1;

    /* 计算音频时长 */
    double duration_sec = (double)num_samples / sample_rate;
    printf("  [Init] Generated %d samples of 1kHz sine wave PCM data\n", num_samples);
    printf("  [Init] Buffer size: %zu bytes, Duration: %.3f seconds\n",
           sizeof(g_pcm_buffer), duration_sec);
    printf("  [Init] Sample range: [%d, %d] (first 3 samples: %d, %d, %d)\n",
           (int)(-amplitude), (int)amplitude,
           samples[0], samples[1], samples[2]);
}

/* ========================================
 * 测试用例 - 基础功能测试
 * ======================================== */

/**
 * @brief 测试基本的流式播放启动
 */
void test_play_stream_basic(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    reset_stream_callback_state();
    app_player_register_callback(player, stream_event_callback, NULL);

    printf("  [Test] Starting stream playback (16kHz, mono, 16-bit)\n");
    int ret = app_player_play_stream(player, 16000, 1, 16);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "play_stream with valid params should return OK");

    /* 等待播放器准备完成 */
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 结束流式播放 */
    app_player_finish_stream(player);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试写入流数据
 */
void test_write_stream_basic(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始流式播放 */
    int ret = app_player_play_stream(player, 16000, 1, 16);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);

    /* 等待准备完成 */
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 初始化全局PCM缓冲区 */
    init_pcm_buffer();

    printf("  [Test] Writing %zu bytes of PCM data\n", sizeof(g_pcm_buffer));
    int written = app_player_write_stream(player, g_pcm_buffer, sizeof(g_pcm_buffer), 1000);
    printf("  [Test] write_stream returns: %d bytes\n", written);

    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, written,
                                         "write_stream should return non-negative value");

    /* 结束流式播放 */
    app_player_finish_stream(player);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试完整的流式播放流程
 */
void test_stream_complete_workflow(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    reset_stream_callback_state();
    app_player_register_callback(player, stream_event_callback, NULL);

    printf("  [Test] Starting complete stream workflow\n");

    /* 1. 开始流式播放 */
    int ret = app_player_play_stream(player, 16000, 1, 16);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 2. 初始化全局PCM缓冲区并写入多块数据 */
    init_pcm_buffer();

    /* 写入完整缓冲区，产生约0.5秒的音频 */
    int total_written = 0;
    const int chunk_size = 4096;  /* 每次写入4KB */
    const int num_chunks = sizeof(g_pcm_buffer) / chunk_size;

    for (int i = 0; i < num_chunks; i++) {
        printf("  [Test] Writing chunk %d/%d (%d bytes)\n", i + 1, num_chunks, chunk_size);
        int written = app_player_write_stream(player,
                                               g_pcm_buffer + (i * chunk_size),
                                               chunk_size,
                                               1000);
        if (written > 0) {
            total_written += written;
            printf("  [Test] Chunk %d: wrote %d bytes (total: %d bytes)\n",
                   i + 1, written, total_written);
        } else {
            printf("  [Test] Warning: Chunk %d write failed: %d\n", i + 1, written);
        }
        TEST_ASSERT_GREATER_OR_EQUAL(0, written);
        wait_ms(20);  /* 短暂延迟，模拟实时数据 */
    }

    printf("  [Test] Total written: %d bytes (%.3f seconds of audio)\n",
           total_written, (double)total_written / (16000.0 * 2.0));

    /* 3. 结束流式播放 */
    printf("  [Test] Finishing stream playback\n");
    ret = app_player_finish_stream(player);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "finish_stream should return OK");

    /* 等待播放完成 */
    printf("  [Test] Waiting for playback to complete...\n");
    wait_ms(TEST_WAIT_LONG_MS);

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试不同采样率的流式播放
 */
void test_stream_different_sample_rates(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 测试常见的采样率 */
    uint32_t sample_rates[] = {8000, 16000, 24000, 48000};

    for (int i = 0; i < sizeof(sample_rates) / sizeof(sample_rates[0]); i++) {
        printf("  [Test] Testing sample rate: %u Hz\n", sample_rates[i]);

        int ret = app_player_play_stream(player, sample_rates[i], 1, 16);
        TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                                  "play_stream should succeed for common sample rates");

        wait_ms(TEST_WAIT_SHORT_MS);
        app_player_finish_stream(player);
        wait_ms(TEST_WAIT_MEDIUM_MS);
    }

    /* 清理 */
    app_player_destroy(player);
}

/* ========================================
 * 测试用例 - 参数验证测试
 * ======================================== */

/**
 * @brief 测试对NULL播放器调用流式接口
 */
void test_stream_null_player(void)
{
    printf("  [Test] Testing stream APIs with NULL player\n");

    /* play_stream with NULL */
    int ret1 = app_player_play_stream(NULL, 16000, 1, 16);
    printf("  [Test] play_stream on NULL player returns: %s\n",
           error_to_string(ret1));
    TEST_ASSERT_EQUAL(APP_PLAYER_ERR_INVALID_PARAM, ret1);

    /* write_stream with NULL */
    uint8_t test_data[100] = {0};
    int ret2 = app_player_write_stream(NULL, test_data, 100, 1000);
    printf("  [Test] write_stream on NULL player returns: %d\n", ret2);
    TEST_ASSERT_LESS_THAN(0, ret2);

    /* finish_stream with NULL */
    int ret3 = app_player_finish_stream(NULL);
    printf("  [Test] finish_stream on NULL player returns: %s\n",
           error_to_string(ret3));
    TEST_ASSERT_EQUAL(APP_PLAYER_ERR_INVALID_PARAM, ret3);
}

/**
 * @brief 测试write_stream传NULL数据
 */
void test_write_stream_null_data(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    printf("  [Test] Testing write_stream with NULL data\n");
    int ret = app_player_write_stream(player, NULL, 100, 1000);
    printf("  [Test] write_stream with NULL data returns: %d\n", ret);

    TEST_ASSERT_LESS_THAN_MESSAGE(0, ret,
                                  "write_stream with NULL data should return error");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试不支持的通道数
 */
void test_stream_unsupported_channels(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    printf("  [Test] Testing unsupported channel count (stereo)\n");
    int ret = app_player_play_stream(player, 16000, 2, 16);
    printf("  [Test] play_stream with 2 channels returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_NOT_SUPPORTED, ret,
                              "Stereo (2 channels) should not be supported");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试不支持的位深度
 */
void test_stream_unsupported_bits(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 测试8位 */
    printf("  [Test] Testing unsupported bit depth (8-bit)\n");
    int ret1 = app_player_play_stream(player, 16000, 1, 8);
    printf("  [Test] play_stream with 8 bits returns: %s\n", error_to_string(ret1));
    TEST_ASSERT_EQUAL(APP_PLAYER_ERR_NOT_SUPPORTED, ret1);

    /* 测试24位 */
    printf("  [Test] Testing unsupported bit depth (24-bit)\n");
    int ret2 = app_player_play_stream(player, 16000, 1, 24);
    printf("  [Test] play_stream with 24 bits returns: %s\n", error_to_string(ret2));
    TEST_ASSERT_EQUAL(APP_PLAYER_ERR_NOT_SUPPORTED, ret2);

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试无效采样率
 */
void test_stream_invalid_sample_rate(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    printf("  [Test] Testing invalid sample rate (0)\n");
    int ret = app_player_play_stream(player, 0, 1, 16);
    printf("  [Test] play_stream with 0 sample rate returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_PARAM, ret,
                              "Sample rate of 0 should return INVALID_PARAM");

    /* 清理 */
    app_player_destroy(player);
}

/* ========================================
 * 测试用例 - 状态与顺序测试
 * ======================================== */

/**
 * @brief 测试未调用play_stream就调用write_stream
 */
void test_write_stream_without_play_stream(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    uint8_t test_data[100] = {0};

    printf("  [Test] Calling write_stream without play_stream first\n");
    int ret = app_player_write_stream(player, test_data, sizeof(test_data), 1000);
    printf("  [Test] write_stream returns: %d\n", ret);

    TEST_ASSERT_LESS_THAN_MESSAGE(0, ret,
                                  "write_stream without play_stream should return error");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试未调用play_stream就调用finish_stream
 */
void test_finish_stream_without_play_stream(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    printf("  [Test] Calling finish_stream without play_stream first\n");
    int ret = app_player_finish_stream(player);
    printf("  [Test] finish_stream returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_STATE, ret,
                              "finish_stream without play_stream should return INVALID_STATE");

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试多次调用finish_stream
 */
void test_finish_stream_twice(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始流式播放 */
    int ret = app_player_play_stream(player, 16000, 1, 16);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 第一次finish */
    printf("  [Test] First finish_stream call\n");
    ret = app_player_finish_stream(player);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);
    wait_ms(TEST_WAIT_SHORT_MS);

    /* 第二次finish */
    printf("  [Test] Second finish_stream call\n");
    ret = app_player_finish_stream(player);
    printf("  [Test] Second finish_stream returns: %s\n", error_to_string(ret));

    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_INVALID_STATE, ret,
                              "Second finish_stream should return INVALID_STATE");

    /* 清理 */
    wait_ms(TEST_WAIT_MEDIUM_MS);
    app_player_destroy(player);
}

/* ========================================
 * 测试用例 - 边界与压力测试
 * ======================================== */

/**
 * @brief 测试写入大量数据
 */
void test_stream_large_data(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    int ret = app_player_play_stream(player, 16000, 1, 16);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 写入较大的数据块 (16KB) */
    uint8_t *large_buffer = (uint8_t *)malloc(16384);
    TEST_ASSERT_NOT_NULL(large_buffer);
    memset(large_buffer, 0, 16384);

    printf("  [Test] Writing large data block (16KB)\n");
    int written = app_player_write_stream(player, large_buffer, 16384, 5000);
    printf("  [Test] write_stream returns: %d bytes\n", written);

    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, written,
                                         "write_stream should handle large data");

    free(large_buffer);

    /* 结束流式播放 */
    app_player_finish_stream(player);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试写入零字节数据
 */
void test_stream_write_zero_bytes(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    int ret = app_player_play_stream(player, 16000, 1, 16);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    uint8_t test_data[1] = {0};

    printf("  [Test] Writing 0 bytes\n");
    int written = app_player_write_stream(player, test_data, 0, 1000);
    printf("  [Test] write_stream with 0 size returns: %d\n", written);

    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, written,
                                         "write_stream with 0 size should not fail");

    /* 结束流式播放 */
    app_player_finish_stream(player);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试超时参数
 */
void test_stream_write_timeout(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    int ret = app_player_play_stream(player, 16000, 1, 16);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    uint8_t test_data[256];
    memset(test_data, 0, sizeof(test_data));

    /* 测试不同的超时值 */
    printf("  [Test] Testing write_stream with various timeout values\n");

    int written1 = app_player_write_stream(player, test_data, sizeof(test_data), 0);
    printf("  [Test] timeout=0: %d bytes written\n", written1);

    int written2 = app_player_write_stream(player, test_data, sizeof(test_data), 100);
    printf("  [Test] timeout=100: %d bytes written\n", written2);

    int written3 = app_player_write_stream(player, test_data, sizeof(test_data), 5000);
    printf("  [Test] timeout=5000: %d bytes written\n", written3);

    /* 结束流式播放 */
    app_player_finish_stream(player);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_destroy(player);
}

/* ========================================
 * 测试用例 - 交互与控制测试
 * ======================================== */

/**
 * @brief 测试流式播放模式下不支持暂停/恢复操作
 */
void test_stream_pause_resume_not_supported(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 开始流式播放并写入一些数据 */
    int ret = app_player_play_stream(player, 16000, 1, 16);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    init_pcm_buffer();
    /* 写入足够的数据以便能听到声音（约0.125秒） */
    printf("  [Test] Writing initial PCM data (4KB)\n");
    int written = app_player_write_stream(player, g_pcm_buffer, 4096, 1000);
    printf("  [Test] Wrote %d bytes\n", written);

    /* 尝试暂停 - 应该返回不支持 */
    printf("  [Test] Attempting to pause during stream mode (should fail)\n");
    ret = app_player_pause(player);
    printf("  [Test] pause returns: %s\n", error_to_string(ret));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_NOT_SUPPORTED, ret,
                              "pause should not be supported in stream mode");

    /* 尝试恢复 - 应该返回不支持 */
    printf("  [Test] Attempting to resume during stream mode (should fail)\n");
    ret = app_player_resume(player);
    printf("  [Test] resume returns: %s\n", error_to_string(ret));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_NOT_SUPPORTED, ret,
                              "resume should not be supported in stream mode");

    /* 结束流式播放 */
    app_player_finish_stream(player);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试流式播放模式下不支持stop操作
 */
void test_stream_stop_not_supported(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    int ret = app_player_play_stream(player, 16000, 1, 16);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    init_pcm_buffer();
    /* 写入足够的数据以便能听到声音（约0.125秒） */
    printf("  [Test] Writing initial PCM data (4KB)\n");
    int written = app_player_write_stream(player, g_pcm_buffer, 4096, 1000);
    printf("  [Test] Wrote %d bytes\n", written);

    /* 尝试停止 - 应该返回不支持 */
    printf("  [Test] Attempting to stop during stream mode (should fail)\n");
    ret = app_player_stop(player);
    printf("  [Test] stop returns: %s\n", error_to_string(ret));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_NOT_SUPPORTED, ret,
                              "stop should not be supported in stream mode");

    /* 尝试同步停止 - 应该返回不支持 */
    printf("  [Test] Attempting to stop_sync during stream mode (should fail)\n");
    ret = app_player_stop_sync(player);
    printf("  [Test] stop_sync returns: %s\n", error_to_string(ret));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_NOT_SUPPORTED, ret,
                              "stop_sync should not be supported in stream mode");

    /* 正常结束流式播放 */
    printf("  [Test] Finishing stream playback normally\n");
    ret = app_player_finish_stream(player);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试流式播放模式下不支持seek操作
 */
void test_stream_seek_not_supported(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    int ret = app_player_play_stream(player, 16000, 1, 16);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    init_pcm_buffer();
    /* 写入足够的数据以便能听到声音（约0.125秒） */
    printf("  [Test] Writing initial PCM data (4KB)\n");
    int written = app_player_write_stream(player, g_pcm_buffer, 4096, 1000);
    printf("  [Test] Wrote %d bytes\n", written);

    /* 尝试seek - 应该返回不支持 */
    printf("  [Test] Attempting to seek during stream mode (should fail)\n");
    ret = app_player_seek(player, 1000);
    printf("  [Test] seek returns: %s\n", error_to_string(ret));
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_ERR_NOT_SUPPORTED, ret,
                              "seek should not be supported in stream mode");

    /* 正常结束流式播放 */
    printf("  [Test] Finishing stream playback normally\n");
    ret = app_player_finish_stream(player);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 */
    app_player_destroy(player);
}

/**
 * @brief 测试流式播放后重新使用URL播放
 */
void test_stream_then_url_playback(void)
{
    app_player_t *player = app_player_create(TEST_PLAYER_NAME);
    TEST_ASSERT_NOT_NULL(player);

    /* 先进行流式播放 */
    printf("  [Test] First: stream playback\n");
    int ret = app_player_play_stream(player, 16000, 1, 16);
    TEST_ASSERT_EQUAL(APP_PLAYER_OK, ret);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 结束流式播放 */
    app_player_finish_stream(player);
    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 然后使用URL播放 */
    printf("  [Test] Second: URL playback\n");
    ret = app_player_play(player, TEST_URL);
    TEST_ASSERT_EQUAL_MESSAGE(APP_PLAYER_OK, ret,
                              "Should be able to play URL after stream playback");

    wait_ms(TEST_WAIT_MEDIUM_MS);

    /* 清理 - 现在可以使用 stop，因为已经退出流式模式 */
    app_player_stop_sync(player);
    app_player_destroy(player);
}

/* ========================================
 * 测试运行器
 * ======================================== */

void run_player_stream_tests(void)
{
    /* 基础功能测试 */
    RUN_TEST(test_play_stream_basic);
    RUN_TEST(test_write_stream_basic);
    RUN_TEST(test_stream_complete_workflow);
    RUN_TEST(test_stream_different_sample_rates);

    /* 参数验证测试 */
    RUN_TEST(test_stream_null_player);
    RUN_TEST(test_write_stream_null_data);
    RUN_TEST(test_stream_unsupported_channels);
    RUN_TEST(test_stream_unsupported_bits);
    RUN_TEST(test_stream_invalid_sample_rate);

    /* 状态与顺序测试 */
    RUN_TEST(test_write_stream_without_play_stream);
    RUN_TEST(test_finish_stream_without_play_stream);
    RUN_TEST(test_finish_stream_twice);

    /* 边界与压力测试 */
    RUN_TEST(test_stream_large_data);
    RUN_TEST(test_stream_write_zero_bytes);
    RUN_TEST(test_stream_write_timeout);

    /* 流式播放限制测试 - 验证不支持的操作 */
    RUN_TEST(test_stream_pause_resume_not_supported);
    RUN_TEST(test_stream_stop_not_supported);
    RUN_TEST(test_stream_seek_not_supported);
    RUN_TEST(test_stream_then_url_playback);
}
