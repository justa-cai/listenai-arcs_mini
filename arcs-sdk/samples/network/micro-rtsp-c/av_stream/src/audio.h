#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>

/* 音频相关配置 */
#define AUDIO_SAMPLE_RATE   8000
#define AUDIO_CHANNELS      1
#define AUDIO_FRAME_SIZE    320  // 40ms @ 8kHz (8000 * 0.04 = 320)
#define AUDIO_DEVICE_NAME   "audio0"
#define AUDIO_QUEUE_LENGTH  25   // 音频队列长度,可缓存25帧数据

/* 音频队列数据结构 */
typedef struct {
    int16_t pcm_data[AUDIO_FRAME_SIZE];  // 原始PCM数据
    uint32_t samples;                     // 样本数
    uint32_t timestamp;                   // 时间戳
} audio_frame_t;

/**
 * @brief 初始化音频模块
 * @return 0:成功, -1:失败
 */
int audio_init(void);

/**
 * @brief 启动音频推流
 */
void audio_start_streaming(void);

/**
 * @brief 停止音频推流
 */
void audio_stop_streaming(void);

/**
 * @brief 从队列获取PCM音频数据并编码为 μ-law
 * @param buffer 输出缓冲区指针
 * @param len 输出数据长度
 * @return 0:成功, -1:失败
 */
int audio_capture_frame(uint8_t **buffer, uint32_t *len);

/**
 * @brief 清空音频队列
 */
void audio_clear_queue(void);

#endif /* AUDIO_H */
