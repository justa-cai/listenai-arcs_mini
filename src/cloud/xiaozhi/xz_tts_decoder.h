/**
 * @file xz_tts_decoder.h
 * @brief 小智云端 TTS 解码器
 * @note 使用独立线程处理 Opus 解码，避免占用 WebSocket 线程堆栈
 */

#ifndef __XZ_TTS_DECODER_H__
#define __XZ_TTS_DECODER_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** TTS 解码器句柄 */
typedef struct xz_tts_decoder_s *xz_tts_decoder_t;

/** TTS 播放器句柄 (前向声明) */
struct xz_tts_player_s;

/** TTS 解码器配置 */
typedef struct {
    int sample_rate;    /**< 采样率 (Hz): 16000, 24000 */
    int channels;       /**< 声道数: 1 */
} xz_tts_decoder_config_t;

/**
 * @brief 创建 TTS 解码器
 * @param config 解码器配置
 * @param player TTS 播放器
 * @return 解码器句柄，失败返回 NULL
 */
xz_tts_decoder_t xz_tts_decoder_create(const xz_tts_decoder_config_t *config,
                                      struct xz_tts_player_s *player);

/**
 * @brief 销毁 TTS 解码器
 * @param decoder 解码器句柄
 */
void xz_tts_decoder_destroy(xz_tts_decoder_t decoder);

/**
 * @brief 开始解码
 * @param decoder 解码器句柄
 * @return 0 成功, -1 失败
 */
int xz_tts_decoder_start(xz_tts_decoder_t decoder);

/**
 * @brief 停止解码
 * @param decoder 解码器句柄
 * @return 0 成功, -1 失败
 */
int xz_tts_decoder_stop(xz_tts_decoder_t decoder);

/**
 * @brief 写入 Opus 数据
 * @param decoder 解码器句柄
 * @param opus_data Opus 编码数据
 * @param len 数据长度
 * @return 0 成功, -1 失败
 */
int xz_tts_decoder_write(xz_tts_decoder_t decoder, const uint8_t *opus_data, uint32_t len);

/**
 * @brief 结束流
 * @param decoder 解码器句柄
 * @return 0 成功, -1 失败
 */
int xz_tts_decoder_end_stream(xz_tts_decoder_t decoder);

/**
 * @brief 检查是否正在解码
 * @param decoder 解码器句柄
 * @return true 正在解码, false 未解码
 */
bool xz_tts_decoder_is_decoding(xz_tts_decoder_t decoder);

#ifdef __cplusplus
}
#endif

#endif /* __XZ_TTS_DECODER_H__ */
