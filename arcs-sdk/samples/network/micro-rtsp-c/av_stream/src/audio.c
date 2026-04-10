#include "audio.h"
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include "lisa_audio.h"

#define TAG "audio"
#include <lisa_log.h>

/* 音频全局变量 */
static lisa_device_t *g_audio_dev = NULL;
static uint8_t g_audio_buffer[AUDIO_FRAME_SIZE];      // μ-law 编码后的缓冲区

/* 音频队列相关 */
static QueueHandle_t g_audio_queue = NULL;            // 音频数据队列
static volatile bool g_is_streaming = false;          // 推流状态标志

/**
 * @brief PCM 16-bit 转 μ-law 编码
 */
static uint8_t pcm16_to_ulaw(int16_t pcm_val)
{
    const int16_t CLIP = 32635;
    const int16_t BIAS = 0x84;

    uint8_t sign = (pcm_val < 0) ? 0x80 : 0;
    if (sign) pcm_val = (int16_t)(-pcm_val);
    if (pcm_val > CLIP) pcm_val = CLIP;

    pcm_val = (int16_t)(pcm_val + BIAS);
    int exponent = 7;
    for (int exp_mask = 0x4000; (pcm_val & exp_mask) == 0 && exponent > 0;
         exp_mask >>= 1, exponent--);

    int mantissa = (pcm_val >> (exponent + 3)) & 0x0F;
    uint8_t ulaw = (uint8_t)(~(sign | (exponent << 4) | mantissa));

    return ulaw;
}

/**
 * @brief 音频回调函数 - 从麦克风接收原始PCM数据并放入队列
 */
static void audio_callback(const lisa_audio_event_t *event, void *user_data)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* 只有在推流状态下才处理录音数据 */
    if (!g_is_streaming) {
        return;
    }

    /* 处理录音数据 */
    if (event->record_buffer && event->record_samples > 0 && g_audio_queue != NULL) {
        uint32_t samples = event->record_samples;

        /* 限制最大样本数 */
        if (samples > AUDIO_FRAME_SIZE) {
            samples = AUDIO_FRAME_SIZE;
        }

        /* 准备音频帧数据 - 只存储原始PCM数据 */
        audio_frame_t frame;
        frame.samples = samples;
        frame.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* 复制原始PCM数据 */
        memcpy(frame.pcm_data, event->record_buffer, samples * sizeof(int16_t));

        /* 尝试将数据放入队列(非阻塞) */
        if (xQueueSendFromISR(g_audio_queue, &frame, &xHigherPriorityTaskWoken) != pdTRUE) {
            LOGI("Audio queue full, dropping oldest frame");
            /* 队列满,丢弃最旧的数据 */
            audio_frame_t dummy;
            xQueueReceiveFromISR(g_audio_queue, &dummy, &xHigherPriorityTaskWoken);
            xQueueSendFromISR(g_audio_queue, &frame, &xHigherPriorityTaskWoken);
        }

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief 初始化音频模块
 * @return 0:成功, -1:失败
 */
int audio_init(void)
{
    int ret;

    LOGI("=== Initializing Audio Module ===");

    /* 创建音频队列 */
    g_audio_queue = xQueueCreate(AUDIO_QUEUE_LENGTH, sizeof(audio_frame_t));
    if (g_audio_queue == NULL) {
        LOGE("Failed to create audio queue");
        return -1;
    }
    LOGI("Audio queue created successfully");

    /* 初始化音频设备 */
    g_audio_dev = lisa_device_get(AUDIO_DEVICE_NAME);
    if (!g_audio_dev) {
        LOGE("Failed to get audio device");
        return -1;
    }
    LOGI("Audio device acquired successfully");

    /* 注册音频回调 */
    ret = lisa_audio_register_callback(g_audio_dev, audio_callback, NULL);
    if (ret != 0) {
        LOGE("Failed to register audio callback: %d", ret);
        return -1;
    }
    LOGI("Audio callback registered successfully");

    /* 配置录音参数 */
    lisa_audio_record_config_t record_config = {
        .format = {
            .sample_rate = LISA_AUDIO_RATE_8K,   /* 8kHz 采样率 */
            .channels = LISA_AUDIO_CH_LEFT,      /* 单声道 */
            .sample_bits = LISA_AUDIO_BIT_16,    /* 16位采样 */
        },
        .gain = {
            .analog_gain = 16,   /* 模拟增益 (参考值) */
            .digital_gain = 8,   /* 数字增益 (参考值) */
        },
        .differential_input = true,
        .enable_hpf = true,      /* 启用高通滤波器 */
    };

    ret = lisa_audio_record_config(g_audio_dev, &record_config);
    if (ret != 0) {
        LOGE("Failed to configure audio recording: %d", ret);
        return -1;
    }
    LOGI("Audio recording configured successfully");

    /* 启动录音 */
    ret = lisa_audio_record_start(g_audio_dev);
    if (ret != 0) {
        LOGE("Failed to start audio recording: %d", ret);
        return -1;
    }
    LOGI("Audio recording started successfully");

    return 0;
}

/**
 * @brief 启动音频推流
 */
void audio_start_streaming(void)
{
    g_is_streaming = true;
    LOGI("Audio streaming started");
}

/**
 * @brief 停止音频推流
 */
void audio_stop_streaming(void)
{
    g_is_streaming = false;
    LOGI("Audio streaming stopped");
}

/**
 * @brief 从队列获取PCM音频数据并编码为 μ-law
 * @param buffer 输出缓冲区指针
 * @param len 输出数据长度
 * @return 0:成功, -1:失败
 */
int audio_capture_frame(uint8_t **buffer, uint32_t *len)
{
    static audio_frame_t frame;

    /* 从队列中读取音频帧(非阻塞) */
    if (g_audio_queue == NULL || xQueueReceive(g_audio_queue, &frame, 0) != pdTRUE) {
        return -1;  /* 队列为空或未初始化 */
    }

    /* 将 PCM 数据转换为 μ-law */
    for (uint32_t i = 0; i < frame.samples; i++) {
        g_audio_buffer[i] = pcm16_to_ulaw(frame.pcm_data[i]);
    }

    *buffer = g_audio_buffer;
    *len = frame.samples;

    return 0;
}

/**
 * @brief 清空音频队列
 */
void audio_clear_queue(void)
{
    if (g_audio_queue != NULL) {
        audio_frame_t dummy;
        while (xQueueReceive(g_audio_queue, &dummy, 0) == pdTRUE) {
            /* 清空队列 */
        }
        LOGI("Audio queue cleared");
    }
}
