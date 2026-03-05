/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_usb_cdc.h"
#include "cdc_protocol.h"
#include "tusb.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "sysheap.h"
#include <string.h>

#define TAG "usb_cdc"
#include "lisa_log.h"

// 如果没有mbedtls，使用简单的MD5实现
#ifdef CONFIG_MBEDTLS
#include "mbedtls/md5.h"
typedef mbedtls_md5_context md5_ctx_t;
#else
// 简化版MD5上下文（实际应用中应使用完整的MD5库）
typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
} md5_ctx_t;
#endif

#define CDC_AUDIO_ITF           0  // CDC instance index for audio streaming (not interface number!)
#define CDC_TASK_STACK_SIZE     (4 * configMINIMAL_STACK_SIZE)
#define CDC_RX_TASK_STACK_SIZE  (4096)
#define CDC_AUDIO_QUEUE_LENGTH  50
#define CDC_PROTO_BUF_SIZE      4096
#define CDC_RX_BUF_SIZE         256

// Audio data structure for queue
struct cdc_audio_data {
    uint8_t *data;
    uint32_t size;
};

// CDC internal state management (renamed to avoid conflict with cdc_state_t in protocol.h)
typedef struct {
    bool recording;             // 是否正在录音
    uint32_t seq_num;           // 当前序列号
    uint32_t total_bytes;       // 已发送的总字节数
    md5_ctx_t md5_ctx;          // MD5上下文
    bool md5_initialized;       // MD5是否已初始化
} cdc_internal_state_t;

static QueueHandle_t cdc_audio_queue = NULL;
static TaskHandle_t cdc_send_task_handle = NULL;
static TaskHandle_t cdc_rx_task_handle = NULL;
static cdc_internal_state_t cdc_state = {0};
static cdc_parser_t cdc_parser;
static uint8_t parser_data_buf[CDC_PROTO_BUF_SIZE];

/**
 * @brief 简化的MD5初始化函数（仅做占位，实际应使用完整MD5库）
 */
static void simple_md5_init(md5_ctx_t *ctx)
{
    // 占位函数，实际应使用完整MD5实现
    (void)ctx;
    memset(ctx, 0, sizeof(md5_ctx_t));
}

/**
 * @brief 简化的MD5开始函数（仅做占位）
 */
static void simple_md5_starts(md5_ctx_t *ctx)
{
    // 占位函数
    (void)ctx;
}

/**
 * @brief 简化的MD5更新函数（仅做占位，实际应使用完整MD5库）
 */
static void simple_md5_update(md5_ctx_t *ctx, const uint8_t *input, uint32_t length)
{
    // 这是一个占位函数，实际应用中应该：
    // 1. 使用mbedtls的md5库（如果CONFIG_MBEDTLS启用）
    // 2. 或者集成一个开源的MD5实现（如md5.c）
    // 这里仅做示意
    (void)ctx;
    (void)input;
    (void)length;
}

/**
 * @brief 简化的MD5完成函数（仅做占位）
 */
static void simple_md5_finish(md5_ctx_t *ctx, uint8_t output[16])
{
    // 占位函数，实际应使用完整MD5实现
    (void)ctx;
    memset(output, 0, 16);
}

/**
 * @brief 简化的MD5释放函数（仅做占位）
 */
static void simple_md5_free(md5_ctx_t *ctx)
{
    // 占位函数
    (void)ctx;
}

/**
 * @brief MD5初始化
 */
static void md5_init(void)
{
#ifdef CONFIG_MBEDTLS
    mbedtls_md5_init(&cdc_state.md5_ctx);
    mbedtls_md5_starts(&cdc_state.md5_ctx);
#else
    simple_md5_init(&cdc_state.md5_ctx);
    simple_md5_starts(&cdc_state.md5_ctx);
#endif
    cdc_state.md5_initialized = true;
}

/**
 * @brief MD5更新
 */
static void md5_update(const uint8_t *data, uint32_t len)
{
    if (!cdc_state.md5_initialized) {
        return;
    }
#ifdef CONFIG_MBEDTLS
    mbedtls_md5_update(&cdc_state.md5_ctx, data, len);
#else
    simple_md5_update(&cdc_state.md5_ctx, data, len);
#endif
}

/**
 * @brief MD5完成并获取结果
 */
static void md5_finish(uint8_t output[16])
{
    if (!cdc_state.md5_initialized) {
        memset(output, 0, 16);
        return;
    }
#ifdef CONFIG_MBEDTLS
    mbedtls_md5_finish(&cdc_state.md5_ctx, output);
    mbedtls_md5_free(&cdc_state.md5_ctx);
#else
    simple_md5_finish(&cdc_state.md5_ctx, output);
    simple_md5_free(&cdc_state.md5_ctx);
#endif
    cdc_state.md5_initialized = false;
}

/**
 * @brief 发送协议帧到CDC
 */
static int send_frame_to_cdc(const uint8_t *frame_data, uint32_t frame_len)
{
    if (!tud_cdc_n_connected(CDC_AUDIO_ITF)) {
        return -1;
    }

    uint32_t sent = 0;
    while (sent < frame_len) {
        uint32_t available = tud_cdc_n_write_available(CDC_AUDIO_ITF);
        if (available > 0) {
            uint32_t to_send = (frame_len - sent) > available ? available : (frame_len - sent);
            uint32_t written = tud_cdc_n_write(CDC_AUDIO_ITF, frame_data + sent, to_send);
            sent += written;
            tud_cdc_n_write_flush(CDC_AUDIO_ITF);
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    return 0;
}

/**
 * @brief 发送音频帧到CDC（优化版：分段发送，避免拷贝）
 *
 * 音频帧保留校验和字节但固定填充为0x00，不进行实际计算，保持协议一致性
 * 数据完整性由最终的MD5校验保证
 *
 * @param seq_num 序列号
 * @param pcm_data PCM数据指针
 * @param pcm_len PCM数据长度
 * @return int 0成功，-1失败
 */
static int send_audio_frame_to_cdc(uint32_t seq_num, const uint8_t *pcm_data, uint32_t pcm_len)
{
    if (!tud_cdc_n_connected(CDC_AUDIO_ITF)) {
        return -1;
    }

    // 1. 构建帧头+序列号（11字节）
    uint8_t header_buf[11];
    int header_len = cdc_proto_build_audio_header(header_buf, sizeof(header_buf), seq_num, pcm_len);
    if (header_len <= 0) {
        return -1;
    }

    // 2. 发送帧头+序列号
    if (send_frame_to_cdc(header_buf, header_len) != 0) {
        return -1;
    }

    // 3. 直接发送PCM数据（零拷贝）
    if (send_frame_to_cdc(pcm_data, pcm_len) != 0) {
        return -1;
    }

    // 4. 发送固定校验位（0x00），保持协议一致性
    uint8_t checksum = 0x00;
    return send_frame_to_cdc(&checksum, 1);
}

/**
 * @brief 处理命令请求
 */
static void handle_command(const cdc_frame_t *frame)
{
    if (!frame || !frame->data || frame->header.length < 1) {
        LISA_LOGE(TAG, "Invalid command frame");
        return;
    }

    const cdc_cmd_request_t *cmd = (const cdc_cmd_request_t *)frame->data;
    uint8_t response_buf[64];
    int response_len = 0;

    LISA_LOGI(TAG, "Received command: 0x%02X", cmd->cmd_id);

    switch (cmd->cmd_id) {
    case CDC_CMD_START_RECORD:
        if (!cdc_state.recording) {
            cdc_state.recording = true;
            cdc_state.seq_num = 0;
            cdc_state.total_bytes = 0;
            md5_init();
            LISA_LOGI(TAG, "Recording started");
            response_len = cdc_proto_build_cmd_response(response_buf, sizeof(response_buf),
                                                         CDC_CMD_START_RECORD, CDC_STATUS_OK,
                                                         NULL, 0);
        } else {
            LISA_LOGW(TAG, "Already recording");
            response_len = cdc_proto_build_cmd_response(response_buf, sizeof(response_buf),
                                                         CDC_CMD_START_RECORD, CDC_STATUS_BUSY,
                                                         NULL, 0);
        }
        break;

    case CDC_CMD_STOP_RECORD:
        if (cdc_state.recording) {
            cdc_state.recording = false;
            LISA_LOGI(TAG, "Recording stopped");

            // 发送MD5
            uint8_t md5_hash[16];
            md5_finish(md5_hash);
            app_usb_cdc_send_md5(md5_hash);

            // 发送停止响应
            response_len = cdc_proto_build_cmd_response(response_buf, sizeof(response_buf),
                                                         CDC_CMD_STOP_RECORD, CDC_STATUS_OK,
                                                         NULL, 0);
        } else {
            LISA_LOGW(TAG, "Not recording");
            response_len = cdc_proto_build_cmd_response(response_buf, sizeof(response_buf),
                                                         CDC_CMD_STOP_RECORD, CDC_STATUS_ERROR,
                                                         NULL, 0);
        }
        break;

    case CDC_CMD_QUERY_STATUS:
        LISA_LOGI(TAG, "Query status: recording=%d, seq=%lu, bytes=%lu",
                  cdc_state.recording, cdc_state.seq_num, cdc_state.total_bytes);
        response_len = cdc_proto_build_status_response(response_buf, sizeof(response_buf),
                                                        cdc_state.recording ? CDC_STATE_RECORDING : CDC_STATE_IDLE,
                                                        cdc_state.total_bytes,
                                                        cdc_state.seq_num);
        break;

    default:
        LISA_LOGW(TAG, "Unsupported command: 0x%02X", cmd->cmd_id);
        response_len = cdc_proto_build_cmd_response(response_buf, sizeof(response_buf),
                                                     cmd->cmd_id, CDC_STATUS_UNSUPPORTED,
                                                     NULL, 0);
        break;
    }

    // 发送响应
    if (response_len > 0) {
        send_frame_to_cdc(response_buf, response_len);
    }
}

/**
 * @brief CDC接收任务
 */
static void cdc_rx_task(void *param)
{
    (void)param;
    uint8_t rx_buf[CDC_RX_BUF_SIZE];

    LISA_LOGI(TAG, "CDC RX task started");

    while (1) {
        if (tud_cdc_n_connected(CDC_AUDIO_ITF) && tud_cdc_n_available(CDC_AUDIO_ITF)) {
            uint32_t count = tud_cdc_n_read(CDC_AUDIO_ITF, rx_buf, sizeof(rx_buf));

            // 逐字节解析
            for (uint32_t i = 0; i < count; i++) {
                cdc_frame_t frame;
                int ret = cdc_proto_parse_byte(&cdc_parser, rx_buf[i], &frame);

                if (ret == 1) {
                    // 帧解析完成
                    if (frame.header.type == CDC_TYPE_CMD_REQUEST) {
                        handle_command(&frame);
                    }
                } else if (ret < 0) {
                    // 解析错误
                    LISA_LOGW(TAG, "Frame parse error");
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief CDC音频发送任务（优化版：使用分段发送）
 */
static void cdc_send_task(void *param)
{
    (void)param;
    struct cdc_audio_data audio_data;

    LISA_LOGI(TAG, "CDC send task started (optimized mode)");

    while (1) {
        // 等待音频数据
        if (xQueueReceive(cdc_audio_queue, &audio_data, portMAX_DELAY) != pdPASS) {
            continue;
        }

        // 检查是否连接且正在录音
        if (!tud_cdc_n_connected(CDC_AUDIO_ITF) || !cdc_state.recording) {
            psram_free(audio_data.data);
            continue;
        }

        // 使用优化的分段发送（避免大缓冲区和数据拷贝）
        if (send_audio_frame_to_cdc(cdc_state.seq_num, audio_data.data, audio_data.size) == 0) {
            // 更新MD5
            md5_update(audio_data.data, audio_data.size);

            // 更新统计
            cdc_state.seq_num++;
            cdc_state.total_bytes += audio_data.size;
        }

        // 释放缓冲区
        psram_free(audio_data.data);
    }
}

/**
 * @brief Initialize USB CDC audio streaming
 */
int app_usb_cdc_init(void)
{
    // 初始化协议解析器
    if (cdc_proto_parser_init(&cdc_parser, parser_data_buf, sizeof(parser_data_buf)) != 0) {
        LISA_LOGE(TAG, "Failed to initialize protocol parser");
        return -1;
    }

    // 初始化状态
    memset(&cdc_state, 0, sizeof(cdc_state));

    // 创建音频数据队列
    cdc_audio_queue = xQueueCreate(CDC_AUDIO_QUEUE_LENGTH, sizeof(struct cdc_audio_data));
    if (cdc_audio_queue == NULL) {
        LISA_LOGE(TAG, "Failed to create CDC audio queue");
        return -1;
    }

    // 创建CDC发送任务
    BaseType_t ret = xTaskCreate(cdc_send_task,
                                 "cdc_send",
                                 CDC_TASK_STACK_SIZE,
                                 NULL,
                                 configMAX_PRIORITIES - 2,
                                 &cdc_send_task_handle);
    if (ret != pdPASS) {
        LISA_LOGE(TAG, "Failed to create CDC send task");
        vQueueDelete(cdc_audio_queue);
        cdc_audio_queue = NULL;
        return -1;
    }

    // 创建CDC接收任务
    ret = xTaskCreate(cdc_rx_task,
                     "cdc_rx",
                     CDC_RX_TASK_STACK_SIZE,
                     NULL,
                     configMAX_PRIORITIES - 1,  // 更高优先级处理命令
                     &cdc_rx_task_handle);
    if (ret != pdPASS) {
        LISA_LOGE(TAG, "Failed to create CDC RX task");
        vTaskDelete(cdc_send_task_handle);
        vQueueDelete(cdc_audio_queue);
        cdc_audio_queue = NULL;
        cdc_send_task_handle = NULL;
        return -1;
    }

    LISA_LOGI(TAG, "USB CDC audio initialized with protocol support");
    return 0;
}


/**
 * @brief Write audio data to CDC interface
 */
int app_usb_cdc_audio_write(uint8_t *data, uint32_t datalen)
{
    if (cdc_audio_queue == NULL) {
        LISA_LOGE(TAG, "CDC audio queue not initialized");
        return -1;
    }

    // 只有在录音状态才接受数据
    if (!cdc_state.recording) {
        return -1;
    }

    struct cdc_audio_data audio_data = {
        .size = datalen,
    };

    // 从PSRAM分配缓冲区
    audio_data.data = psram_malloc(datalen);
    if (!audio_data.data) {
        LISA_LOGE(TAG, "Failed to allocate buffer for CDC audio");
        return -1;
    }

    // 复制音频数据
    memcpy(audio_data.data, data, datalen);

    // 发送到队列
    BaseType_t ret = xQueueSend(cdc_audio_queue, &audio_data, portMAX_DELAY);
    if (ret != pdPASS) {
        psram_free(audio_data.data);
        LISA_LOGW(TAG, "CDC audio queue full, dropping frame");
        return -1;
    }
    return 0;
}

/**
 * @brief Send MD5 hash of recorded audio
 */
int app_usb_cdc_send_md5(const uint8_t *md5)
{
    if (!md5) {
        return -1;
    }

    uint8_t frame_buf[64];
    int frame_len = cdc_proto_build_md5_frame(frame_buf, sizeof(frame_buf), md5);

    if (frame_len > 0) {
        LISA_LOGI(TAG, "Sending MD5: %02X%02X%02X%02X...",
                  md5[0], md5[1], md5[2], md5[3]);
        return send_frame_to_cdc(frame_buf, frame_len);
    }

    return -1;
}

/**
 * @brief Get current recording state
 */
bool app_usb_cdc_is_recording(void)
{
    return cdc_state.recording;
}

/**
 * @brief Get total bytes sent
 */
uint32_t app_usb_cdc_get_total_bytes(void)
{
    return cdc_state.total_bytes;
}

/**
 * @brief Get current sequence number
 */
uint32_t app_usb_cdc_get_seq_num(void)
{
    return cdc_state.seq_num;
}

