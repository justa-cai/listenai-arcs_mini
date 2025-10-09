#ifndef LISA_EVS_SDK_RECOGNIZER_H
#define LISA_EVS_SDK_RECOGNIZER_H

#include <stdbool.h>
#include <stdint.h>
#include "lisa_err.h"

typedef struct lisa_evs_recognizer_state {
	uint8_t version[12];
} lisa_evs_recognizer_state_t;

typedef enum {
	// AUDIO_L16_RATE_16000_CHANNELS_1
	EVS_REC_AUDIO_FORMAT_BASE,
	// OPUS
	EVS_REC_AUDIO_FORMAT_OPUS,
	// SPEEX_WB_QUALITY_9
	EVS_REC_AUDIO_FORMAT_SPEEX
} lisa_evs_rec_audio_format_e;

typedef enum {
	// 近场识别，适合3米内的使用场景
	EVS_REC_AUDIO_PROFILE_CLOSE_TALK,
	// 远场识别，适合10米内的使用场景
	EVS_REC_AUDIO_FORMAT_FAR,
	// 语音评测，适合中英文评测场景
	EVS_REC_AUDIO_PROFILE_EVALUATE
} lisa_evs_rec_audio_profile_e;

/** 音频识别参数 */
typedef struct lisa_evs_rec_audio_param_s {
	lisa_evs_rec_audio_format_e format;
	lisa_evs_rec_audio_profile_e profile;
	// 是否使用云端VAD，默认为true
	bool enable_vad;
	// VAD后端点时长，单位为毫秒, 需大于800ms
	int vad_eos;
	uint8_t *reply_key;
	bool translation;
} lisa_evs_rec_audio_param_t;

typedef enum {
	// 评测语言，取值：zh_cn（中文）
	EVS_REC_EVALUATE_ZH_CN,
	// 评测语言，取值：en_us（美式英文
	EVS_REC_EVALUATE_EN_US,
} lisa_evs_rec_evaluate_language;
typedef enum {
	// 篇章朗读,仅英文可用，300个英文字符以内
	EVS_REC_EVALUATE_READ_CHAPTER,
	// 句子朗读, 中英文可用, 一句话，英文建议100个字符以内，中文建议30个字符以内
	EVS_REC_EVALUATE_READ_SENTENCE,
	// 词语朗读, 中英文可用, 词语，不支持标点符号
	EVS_REC_EVALUATE_READ_WORD,
	// 单字朗读, 仅中文可用,单字，不支持标点符号
	EVS_REC_EVALUATE_READ_SYLLABLE,
	EVS_REC_EVALUATE_READ_CHOICE,
} lisa_evs_rec_evaluate_category;

typedef struct lisa_evs_rec_assess_param
{
	lisa_evs_rec_evaluate_language language;
	lisa_evs_rec_evaluate_category category;
	uint8_t* text;
} lisa_evs_rec_evaluate_param_t;

typedef struct
{
	uint8_t* text;
	bool translation;
	bool with_tts;
} lisa_evs_rec_translation_param_t;

typedef struct
{
	uint8_t* text;
	float speed;
	uint8_t volume;
	uint8_t* vcn;
} lisa_evs_rec_tts_param_t;

#define EVS_REC_IMG_PARAM_INIT(...) { \
		.single_line = true, \
		.debug_image = false, \
		.tokenization = false, \
		.debug = false, \
		## __VA_ARGS__ }

typedef struct lisa_evs_rec {
	struct lisa_evs *evs;
	struct lisa_evs_rec_context_s *ctx;
	struct lisa_evs_ws_s *ws;
} lisa_evs_rec_t;

typedef void (*lisa_evs_rec_data_cb)(const uint8_t *const data, uint32_t len);


/**
 * @brief 音频识别开始
 * @param handle 			识别句柄
 * @param req_id 		    请求ID
 * @param param 			音频参数
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_audio_start(
		const lisa_evs_rec_t *const handle, const uint8_t *const req_id, const lisa_evs_rec_audio_param_t *const param);

/**
 * @brief 音频识别结束
 * @param handle            识别句柄
 * @param req_id            请求ID
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_audio_end(const lisa_evs_rec_t *const handle, const uint8_t *const req_id);

/**
 * @brief 音频识别停止
 * @param handle 			识别句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_audio_stop(const lisa_evs_rec_t *const handle);

/**
 * @brief 音频识别取消
 * @param handle 			识别句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_audio_cancel(const lisa_evs_rec_t *const handle);

/**
 * @brief 					发送音频到云端
 * @param  handle           识别句柄
 * @param  audio            音频buffer(>=640字节数据,建议640)
 * @param  len              音频长度
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_send_audio(
		const lisa_evs_rec_t *const handle, const uint8_t *const audio, int len);

/**
 * @brief  口语评测开始
 * @param  handle           句柄
 * @param  req_id           请求id
 * @param  param            音频参数
 * @param  evaluate         评测参数
 * @return lisa_err_t 
 */
lisa_err_t lisa_evs_rec_evaluate_start(
		const lisa_evs_rec_t *const handle, const uint8_t *const req_id, 
		const lisa_evs_rec_audio_param_t *const param, const lisa_evs_rec_evaluate_param_t *const evaluate);

/**
 * @brief  口语评测结束
 * @param  handle           句柄
 * @param  req_id           请求id
 * @return lisa_err_t 
 */
lisa_err_t lisa_evs_rec_evaluate_end(const lisa_evs_rec_t *const handle, const uint8_t *const req_id);

/**
 * @brief  口语评测取消
 * @param  handle           句柄
 * @param  req_id           请求id
 * @return lisa_err_t 
 */
lisa_err_t lisa_evs_rec_evaluate_cancel(const lisa_evs_rec_t *const handle, const uint8_t *const req_id);

/**
 * @brief  文本翻译开始
 * @param  handle           句柄
 * @param  req_id           请求id
 * @param  param            翻译参数
 * @return lisa_err_t 
 */
lisa_err_t lisa_evs_rec_translate_start(
	const lisa_evs_rec_t *const handle, const uint8_t *const req_id,
	const lisa_evs_rec_translation_param_t *const param);

/**
* @brief  文本翻译取消
* @param  handle           句柄
* @param  req_id           请求id
* @return lisa_err_t 
*/
lisa_err_t lisa_evs_rec_translation_cancel(const lisa_evs_rec_t *const handle);

/**
 * @brief  tts开始
 * @param  handle           句柄
 * @param  req_id           请求id
 * @param  param            tts参数
 * @return lisa_err_t 
 */
lisa_err_t lisa_evs_rec_tts_start(
	const lisa_evs_rec_t *const handle, const uint8_t *const req_id,
	const lisa_evs_rec_tts_param_t *const param);

/**
* @brief  tts取消
* @param  handle           句柄
* @param  req_id           请求id
* @return lisa_err_t 
*/
lisa_err_t lisa_evs_rec_tts_cancel(const lisa_evs_rec_t *const handle);

#endif
