/**
 * @file opus.c
 * @brief Opus 编解码性能测试命令
 */

#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include "shell.h"
#include "cmd.h"
#include "stddef.h"
#include "string.h"
#include "lisa_mem.h"
#include "lisa_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "opus_encoder.h"
#include "opus_wrapper.h"
#include "opus/opus_defines.h"

#define TAG "shell-opus"

/* 测试配置 */
#define OPUS_TEST_SAMPLE_RATE   16000
#define OPUS_TEST_CHANNELS      1
#define OPUS_TEST_FRAME_SIZE    320     /* 16kHz / 50 = 320 samples (20ms) */
#define OPUS_TEST_FRAMES        100     /* 测试帧数，共 2 秒音频 */
#define OPUS_TEST_BITRATE       24000   /* 24 kbps */
#define OPUS_TEST_STACK_SIZE    (32 * 1024)  /* 32KB 栈用于测试任务 */

/* 测试类型 */
typedef enum {
    OPUS_TEST_ENCODE = 0,
    OPUS_TEST_DECODE = 1,
    OPUS_TEST_LOOPBACK = 2,
    OPUS_TEST_VERSION = 3,
} opus_test_type_t;

/* 测试参数结构 */
typedef struct {
    opus_test_type_t type;
    int frames;
    SemaphoreHandle_t done_sem;
    int result;
    char output[512];
} opus_test_params_t;

/**
 * @brief 获取系统时间 (毫秒)
 */
static uint64_t get_time_ms(void)
{
    return lisa_os_get_tick_ms();
}

/**
 * @brief 编码测试 (在测试任务中运行)
 */
static void run_encode_test(opus_test_params_t *params)
{
    opus_enc_t encoder = NULL;
    opus_encoder_config_t config;
    int16_t *pcm_data = NULL;
    uint8_t *opus_data = NULL;
    uint64_t start_time, end_time;
    double total_time_ms, avg_time_ms, avg_time_us;
    int i, frames;
    size_t max_packet_size;
    opus_encode_result_t result;
    int offset = 0;

    frames = params->frames > 0 ? params->frames : OPUS_TEST_FRAMES;

    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "=== Opus Encode Test ===\n");
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Sample Rate: %d Hz\n", OPUS_TEST_SAMPLE_RATE);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Channels: %d\n", OPUS_TEST_CHANNELS);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Frame Size: %d samples (%.1f ms)\n", OPUS_TEST_FRAME_SIZE, 20.0);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Bitrate: %d bps\n", OPUS_TEST_BITRATE);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Test Frames: %d (%.1f seconds)\n", frames, frames * 20.0 / 1000);

    /* 配置编码器 */
    memset(&config, 0, sizeof(config));
    config.sample_rate = OPUS_TEST_SAMPLE_RATE;
    config.channels = OPUS_TEST_CHANNELS;
    config.frame_size = OPUS_TEST_FRAME_SIZE;
    config.application = OPUS_APPLICATION_VOIP;
    config.bitrate = OPUS_TEST_BITRATE;
    config.complexity = 0;
    config.vbr = 1;

    /* 创建编码器 */
    encoder = opus_enc_create(&config);
    if (!encoder) {
        offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                          "Error: Failed to create encoder\n");
        params->result = -1;
        return;
    }

    /* 分配测试缓冲区 */
    pcm_data = (int16_t *)lisa_mem_alloc(OPUS_TEST_FRAME_SIZE * OPUS_TEST_CHANNELS * sizeof(int16_t));
    max_packet_size = opus_enc_calc_packet_size(&config, 1);
    opus_data = (uint8_t *)lisa_mem_alloc(max_packet_size);

    if (!pcm_data || !opus_data) {
        offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                          "Error: Failed to allocate buffers\n");
        params->result = -1;
        goto cleanup;
    }

    /* 生成测试 PCM 数据 (1kHz 正弦波) */
    for (i = 0; i < OPUS_TEST_FRAME_SIZE * OPUS_TEST_CHANNELS; i++) {
        pcm_data[i] = (int16_t)(16000.0 * sin(2.0 * 3.1415926535 * 1000.0 * i / OPUS_TEST_SAMPLE_RATE));
    }

    /* 编码测试 */
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "\n--- Encoding Performance ---\n");
    start_time = get_time_ms();

    for (i = 0; i < frames; i++) {
        size_t out_len = max_packet_size;
        result = opus_enc_encode(encoder, pcm_data, OPUS_TEST_FRAME_SIZE, opus_data, &out_len);
        if (result != OPUS_ENCODE_OK) {
            offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                              "Error: Encoding failed at frame %d\n", i);
            goto cleanup;
        }
    }

    end_time = get_time_ms();
    total_time_ms = end_time - start_time;
    avg_time_ms = total_time_ms / frames;
    avg_time_us = avg_time_ms * 1000.0;

    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Total Time: %.2f ms\n", total_time_ms);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Average Time per Frame: %.3f ms (%.1f us)\n", avg_time_ms, avg_time_us);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Frames Per Second: %.1f\n", 1000.0 / avg_time_ms);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "CPU Usage: %.2f%% (Realtime = 20ms/frame)\n", avg_time_ms / 20.0 * 100);

    params->result = 0;

cleanup:
    if (encoder) {
        opus_enc_destroy(encoder);
    }
    if (pcm_data) {
        lisa_mem_free(pcm_data);
    }
    if (opus_data) {
        lisa_mem_free(opus_data);
    }
}

/**
 * @brief 解码测试 (在测试任务中运行)
 */
static void run_decode_test(opus_test_params_t *params)
{
    opus_dec_t decoder = NULL;
    opus_decoder_config_t config;
    int16_t *pcm_data = NULL;
    uint8_t *opus_data = NULL;
    uint64_t start_time, end_time;
    double total_time_ms, avg_time_ms, avg_time_us;
    int i, frames;
    size_t max_packet_size, pcm_size;
    opus_decode_result_t result;
    int offset = 0;

    frames = params->frames > 0 ? params->frames : OPUS_TEST_FRAMES;

    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "=== Opus Decode Test ===\n");
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Sample Rate: %d Hz\n", OPUS_TEST_SAMPLE_RATE);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Channels: %d\n", OPUS_TEST_CHANNELS);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Frame Size: %d samples (%.1f ms)\n", OPUS_TEST_FRAME_SIZE, 20.0);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Test Frames: %d (%.1f seconds)\n", frames, frames * 20.0 / 1000);

    /* 配置解码器 */
    memset(&config, 0, sizeof(config));
    config.sample_rate = OPUS_TEST_SAMPLE_RATE;
    config.channels = OPUS_TEST_CHANNELS;
    config.frame_size = OPUS_TEST_FRAME_SIZE;

    /* 创建解码器 */
    decoder = opus_dec_create(&config);
    if (!decoder) {
        offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                          "Error: Failed to create decoder\n");
        params->result = -1;
        return;
    }

    /* 分配测试缓冲区 */
    max_packet_size = 1276;
    pcm_size = opus_dec_calc_pcm_size(&config, 1);
    opus_data = (uint8_t *)lisa_mem_alloc(max_packet_size);
    pcm_data = (int16_t *)lisa_mem_alloc(pcm_size);

    if (!pcm_data || !opus_data) {
        offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                          "Error: Failed to allocate buffers\n");
        params->result = -1;
        goto cleanup;
    }

    /* 创建一个模拟的 Opus 包 */
    opus_data[0] = 0x00;
    size_t test_packet_size = 1;

    /* 解码测试 */
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "\n--- Decoding Performance ---\n");
    start_time = get_time_ms();

    for (i = 0; i < frames; i++) {
        size_t out_len = pcm_size;
        result = opus_dec_decode(decoder, opus_data, test_packet_size, pcm_data, &out_len);
        if (result != OPUS_DECODE_OK && result != OPUS_DECODE_ERR_CORRUPT) {
            /* 静默处理错误，继续测试 */
        }
    }

    end_time = get_time_ms();
    total_time_ms = end_time - start_time;
    avg_time_ms = total_time_ms / frames;
    avg_time_us = avg_time_ms * 1000.0;

    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Total Time: %.2f ms\n", total_time_ms);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Average Time per Frame: %.3f ms (%.1f us)\n", avg_time_ms, avg_time_us);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Frames Per Second: %.1f\n", 1000.0 / avg_time_ms);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "CPU Usage: %.2f%% (Realtime = 20ms/frame)\n", avg_time_ms / 20.0 * 100);

    params->result = 0;

cleanup:
    if (decoder) {
        opus_dec_destroy(decoder);
    }
    if (pcm_data) {
        lisa_mem_free(pcm_data);
    }
    if (opus_data) {
        lisa_mem_free(opus_data);
    }
}

/**
 * @brief 编解码循环测试 (在测试任务中运行)
 */
static void run_loopback_test(opus_test_params_t *params)
{
    opus_enc_t encoder = NULL;
    opus_dec_t decoder = NULL;
    opus_encoder_config_t enc_config;
    opus_decoder_config_t dec_config;
    int16_t *pcm_in = NULL;
    int16_t *pcm_out = NULL;
    uint8_t *opus_data = NULL;
    uint64_t start_time, end_time;
    double total_time_ms, avg_time_ms, avg_time_us;
    int i, frames;
    size_t max_packet_size, pcm_size;
    opus_encode_result_t enc_result;
    opus_decode_result_t dec_result;
    int offset = 0;

    frames = params->frames > 0 ? params->frames : OPUS_TEST_FRAMES;

    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "=== Opus Loopback Test (Encode + Decode) ===\n");
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Sample Rate: %d Hz\n", OPUS_TEST_SAMPLE_RATE);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Channels: %d\n", OPUS_TEST_CHANNELS);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Frame Size: %d samples (%.1f ms)\n", OPUS_TEST_FRAME_SIZE, 20.0);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Test Frames: %d (%.1f seconds)\n", frames, frames * 20.0 / 1000);

    /* 配置编码器 */
    memset(&enc_config, 0, sizeof(enc_config));
    enc_config.sample_rate = OPUS_TEST_SAMPLE_RATE;
    enc_config.channels = OPUS_TEST_CHANNELS;
    enc_config.frame_size = OPUS_TEST_FRAME_SIZE;
    enc_config.application = OPUS_APPLICATION_VOIP;
    enc_config.bitrate = OPUS_TEST_BITRATE;
    enc_config.complexity = 5;
    enc_config.vbr = 1;

    /* 配置解码器 */
    memset(&dec_config, 0, sizeof(dec_config));
    dec_config.sample_rate = OPUS_TEST_SAMPLE_RATE;
    dec_config.channels = OPUS_TEST_CHANNELS;
    dec_config.frame_size = OPUS_TEST_FRAME_SIZE;

    /* 创建编解码器 */
    encoder = opus_enc_create(&enc_config);
    decoder = opus_dec_create(&dec_config);
    if (!encoder || !decoder) {
        offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                          "Error: Failed to create encoder/decoder\n");
        params->result = -1;
        goto cleanup;
    }

    /* 分配测试缓冲区 */
    pcm_size = OPUS_TEST_FRAME_SIZE * OPUS_TEST_CHANNELS * sizeof(int16_t);
    pcm_in = (int16_t *)lisa_mem_alloc(pcm_size);
    pcm_out = (int16_t *)lisa_mem_alloc(pcm_size);
    max_packet_size = opus_enc_calc_packet_size(&enc_config, 1);
    opus_data = (uint8_t *)lisa_mem_alloc(max_packet_size);

    if (!pcm_in || !pcm_out || !opus_data) {
        offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                          "Error: Failed to allocate buffers\n");
        params->result = -1;
        goto cleanup;
    }

    /* 生成测试 PCM 数据 */
    for (i = 0; i < OPUS_TEST_FRAME_SIZE * OPUS_TEST_CHANNELS; i++) {
        pcm_in[i] = (int16_t)(16000.0 * sin(2.0 * 3.1415926535 * 1000.0 * i / OPUS_TEST_SAMPLE_RATE));
    }

    /* 编解码循环测试 */
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "\n--- Loopback Performance ---\n");
    start_time = get_time_ms();

    for (i = 0; i < frames; i++) {
        size_t out_len;

        /* 编码 */
        out_len = max_packet_size;
        enc_result = opus_enc_encode(encoder, pcm_in, OPUS_TEST_FRAME_SIZE, opus_data, &out_len);
        if (enc_result != OPUS_ENCODE_OK) {
            offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                              "Error: Encoding failed at frame %d\n", i);
            break;
        }

        /* 解码 */
        out_len = pcm_size;
        dec_result = opus_dec_decode(decoder, opus_data, out_len, pcm_out, &out_len);
        if (dec_result != OPUS_DECODE_OK) {
            /* 静默处理错误 */
        }
    }

    end_time = get_time_ms();
    total_time_ms = end_time - start_time;
    avg_time_ms = total_time_ms / frames;
    avg_time_us = avg_time_ms * 1000.0;

    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Total Time: %.2f ms\n", total_time_ms);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Average Time per Frame: %.3f ms (%.1f us)\n", avg_time_ms, avg_time_us);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Frames Per Second: %.1f\n", 1000.0 / avg_time_ms);
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "CPU Usage: %.2f%% (Realtime = 20ms/frame)\n", avg_time_ms / 20.0 * 100);

    params->result = 0;

cleanup:
    if (encoder) {
        opus_enc_destroy(encoder);
    }
    if (decoder) {
        opus_dec_destroy(decoder);
    }
    if (pcm_in) {
        lisa_mem_free(pcm_in);
    }
    if (pcm_out) {
        lisa_mem_free(pcm_out);
    }
    if (opus_data) {
        lisa_mem_free(opus_data);
    }
}

/**
 * @brief 版本测试 (在测试任务中运行)
 */
static void run_version_test(opus_test_params_t *params)
{
    int offset = 0;

    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "=== Opus Library Information ===\n");
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Encoder Version: %s\n", opus_enc_get_version());
    offset += snprintf(params->output + offset, sizeof(params->output) - offset,
                      "Decoder Version: %s\n", opus_dec_get_version());

    params->result = 0;
}

/**
 * @brief 测试任务入口函数
 */
static void opus_test_task(void *pvParameters)
{
    opus_test_params_t *params = (opus_test_params_t *)pvParameters;

    switch (params->type) {
        case OPUS_TEST_ENCODE:
            run_encode_test(params);
            break;
        case OPUS_TEST_DECODE:
            run_decode_test(params);
            break;
        case OPUS_TEST_LOOPBACK:
            run_loopback_test(params);
            break;
        case OPUS_TEST_VERSION:
            run_version_test(params);
            break;
        default:
            snprintf(params->output, sizeof(params->output),
                     "Error: Unknown test type\n");
            params->result = -1;
            break;
    }

    /* 通知完成 */
    if (params->done_sem) {
        xSemaphoreGive(params->done_sem);
    }

    /* 删除任务 */
    vTaskDelete(NULL);
}

/**
 * @brief 启动测试任务
 */
static int start_opus_test(opus_test_type_t type, int frames)
{
    opus_test_params_t *params;
    SemaphoreHandle_t done_sem;
    TaskHandle_t task_handle;

    /* 分配参数 */
    params = (opus_test_params_t *)lisa_mem_alloc(sizeof(opus_test_params_t));
    if (!params) {
        printf("Error: Failed to allocate params\n");
        return -1;
    }

    memset(params, 0, sizeof(opus_test_params_t));
    params->type = type;
    params->frames = frames;
    params->result = -1;

    /* 创建完成信号量 */
    done_sem = xSemaphoreCreateBinary();
    if (!done_sem) {
        lisa_mem_free(params);
        printf("Error: Failed to create semaphore\n");
        return -1;
    }
    params->done_sem = done_sem;

    /* 创建测试任务 */
    if (xTaskCreate(opus_test_task, "opus_test", OPUS_TEST_STACK_SIZE,
                    params, configMAX_PRIORITIES - 2, &task_handle) != pdPASS) {
        vSemaphoreDelete(done_sem);
        lisa_mem_free(params);
        printf("Error: Failed to create test task\n");
        return -1;
    }

    /* 等待测试完成 */
    if (xSemaphoreTake(done_sem, pdMS_TO_TICKS(30000)) != pdTRUE) {
        printf("Error: Test timeout\n");
        vTaskDelete(task_handle);
        vSemaphoreDelete(done_sem);
        lisa_mem_free(params);
        return -1;
    }

    /* 打印结果 */
    printf("%s", params->output);

    /* 清理 */
    vSemaphoreDelete(done_sem);
    lisa_mem_free(params);

    return 0;
}

/**
 * @brief 编码测试命令
 */
static int cmd_opus_encode_test(int argc, char **argv)
{
    int frames = (argc > 0) ? atoi(argv[0]) : OPUS_TEST_FRAMES;
    if (frames <= 0) {
        frames = OPUS_TEST_FRAMES;
    }
    return start_opus_test(OPUS_TEST_ENCODE, frames);
}

/**
 * @brief 解码测试命令
 */
static int cmd_opus_decode_test(int argc, char **argv)
{
    int frames = (argc > 0) ? atoi(argv[0]) : OPUS_TEST_FRAMES;
    if (frames <= 0) {
        frames = OPUS_TEST_FRAMES;
    }
    return start_opus_test(OPUS_TEST_DECODE, frames);
}

/**
 * @brief 编解码循环测试命令
 */
static int cmd_opus_loopback_test(int argc, char **argv)
{
    int frames = (argc > 0) ? atoi(argv[0]) : OPUS_TEST_FRAMES;
    if (frames <= 0) {
        frames = OPUS_TEST_FRAMES;
    }
    return start_opus_test(OPUS_TEST_LOOPBACK, frames);
}

/**
 * @brief 版本命令
 */
static int cmd_opus_version(int argc, char **argv)
{
    return start_opus_test(OPUS_TEST_VERSION, 0);
}

/**
 * @brief 帮助信息
 */
static int cmd_opus_help(int argc, char **argv);

/* 命令表 */
static const struct listen_cmd_t g_opus_cmds[] = {
    {"encode", cmd_opus_encode_test, "opus encode [frames] - Test encode performance (default: 100 frames)"},
    {"decode", cmd_opus_decode_test, "opus decode [frames] - Test decode performance (default: 100 frames)"},
    {"loopback", cmd_opus_loopback_test, "opus loopback [frames] - Test encode+decode loopback (default: 100 frames)"},
    {"version", cmd_opus_version, "opus version - Show Opus library version"},
    {"help", cmd_opus_help, NULL},
};

static int cmd_opus_help(int argc, char **argv)
{
    int cmd_len = sizeof(g_opus_cmds) / sizeof(g_opus_cmds[0]);
    printf("=== Opus Test Commands ===\n");
    for (int i = 0; i < cmd_len; i++) {
        if (g_opus_cmds[i].help != NULL && strcmp(g_opus_cmds[i].name, "help") != 0) {
            printf("%-16s\t:\t%s\n", g_opus_cmds[i].name, g_opus_cmds[i].help);
        }
    }
    return 0;
}

static int opus_cmd_handler(int argc, char **argv)
{
    if (argc == 1) {
        cmd_opus_help(argc, argv);
        return 0;
    }

    for (int i = 0; i < sizeof(g_opus_cmds) / sizeof(g_opus_cmds[0]); i++) {
        if (strcmp(g_opus_cmds[i].name, argv[1]) == 0) {
            return g_opus_cmds[i].exec(argc - 2, argv + 2);
        }
    }

    cmd_opus_help(argc, argv);
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, opus,
                 opus_cmd_handler, opus test command group);
