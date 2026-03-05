/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "lsc_conn.h"
#include "lisa_evt_pub.h"
#include "lisa_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */

#define STRINGS_SESSIONS_CORE_EVT(evt) \
    ((evt == SESSION_STARTED) ? "sessions core started" : \
     (evt == SESSION_GOT_VAD) ? "sessions core got vad" : \
     (evt == SESSION_TTS_URL) ? "sessions core tts url" : \
     (evt == SESSION_IAT_TXT) ? "sessions core iat txt" : \
     (evt == SESSION_REPLY_URL) ? "sessions core reply url" : \
     (evt == SESSION_DRAW_URL) ? "sessions core draw url" : \
     (evt == SESSION_AIUI_CTR) ? "sessions core aiui ctr" : \
     (evt == SESSION_NLP_RAW) ? "sessions core nlp raw" : \
     (evt == SESSION_MUSIC_LISTS) ? "sessions core music lists" : \
     (evt == SESSION_MUSIC_INSTR) ? "sessions core music ctr" : \
	 (evt == SESSION_FINISH) ? "sessions core finish" : \
     (evt == SESSION_ERR_FRAME) ? "sessions core err frame" : \
     (evt == SESSION_TOKEN_INVALIDATION) ? "sessions core token invalidation" : \
	 (evt == SESSION_RESULT_RAW_DATA) ? "sessions core result raw data" : \
     (evt == SESSION_ALARM_INTENT) ? "sessions core alarm intent" : \
     "other evt")
/* clang-format on */

typedef struct {
	// 开启云端ASR识别
	bool enable;
	// 开启云端vad
	bool vad_enable;
	bool oneshot;
	char **words;
	uint32_t words_cnt;
} asr_params_t;

typedef struct {
	// 开启云端下发tts音频链接
	bool enable;

	// 发语音人；
	char vcn[16];

	// 音量
	int volume;

	// 语速
	int speed;

	// 声调
	int pitch;

} tts_params_t;

typedef struct {
	// 开启语义理解和回复
	bool enable;

	char sn[64];
} nlu_params_t;
typedef struct {
	char name[64];
	char intend[64];
} abilities;

typedef struct {
	bool enable;
	struct abilities *abilities;
} nlu_properties;

typedef struct {
	/*
		是否开启全双工模式
	*/
	bool full_duplex;

	/**
	 *	全双工模式下，超时时间，单位：秒
	 *	如果超过该时间没有收到有效数据，则认为会话结束
	 */
	uint32_t full_duplex_timeout_s;

	/*
		单工模式下，超时时间，单位：毫秒
		该时间是指从发起会话的start帧开始计算，以接收到finish帧或作为结束。整个会话的最大时间
	*/
	uint32_t session_timeout;

	/*
		数据类型：
		audio/text
	*/
	char data_type[16];

	/*
		data_type为aduio，必须指定音频格式：
		raw/speex/speed-wb/ico
	*/
	char aue[16];

	/*
		音频数据格式为speex、speex-web时必填
	*/
	int speex_size;

	nlu_properties nlu_properties;
	nlu_params_t nlu_params;
	tts_params_t tts_params;
	asr_params_t asr_params;
} session_params_t;

typedef struct {
	uint32_t rid;
	char sid[64];
	session_params_t params;
	lisa_evt_publisher_t *pub;
	lisa_timer_t *timer;
} session_t;

typedef struct {
	// char url[512];
	char id[64];
	// char all_rate[64];
	// int duration;
	bool playable;
} session_music_item_t;

typedef struct {
	uint32_t cnt;
	session_music_item_t items[];
} session_music_lists_t;

typedef enum {
	// 下一首
	SESSION_MUSIC_INSTR_NEXT,
	// 上一首
	SESSION_MUSIC_INSTR_PAST,
	// 重播
	SESSION_MUSIC_INSTR_REPLAY,
	// 停止播放
	SESSION_MUSIC_INSTR_CLOSE,
	// 音量设置
	SESSION_MUSIC_INSTR_VOL_SET,
} session_music_instr_e;

typedef struct {
	session_music_instr_e instr;
	int arg;
} session_music_instr_t;

typedef enum {
	SESSION_STARTED = BIT(0),
	SESSION_GOT_VAD = BIT(1),
	SESSION_TTS_URL = BIT(2),
	SESSION_IAT_TXT = BIT(3),
	SESSION_REPLY_URL = BIT(4),
	SESSION_DRAW_URL = BIT(5),
	SESSION_AIUI_CTR = BIT(6),
	SESSION_NLP_RAW = BIT(7),
	SESSION_MUSIC_LISTS = BIT(9),
	SESSION_MUSIC_INSTR = BIT(10),
	SESSION_FINISH = BIT(11),
	SESSION_ERR_FRAME = BIT(12),
	SESSION_TOKEN_INVALIDATION = BIT(13),
	SESSION_RESULT_RAW_DATA = BIT(14),
	SESSION_TIMEOUT = BIT(15),
	SESSION_ALARM_INTENT = BIT(16),
	SESSION_IAT_START = BIT(19),
	SESSION_IAT_END = BIT(20),
	SESSION_VPR_INFO = BIT(21),
	SESSION_VPR_FEATURE = BIT(22),
} sessions_event_e;

/**
 * @brief 会话事件回调函数
 *
 * @param evt 事件类型，参考@see sessions_event_e
 * @param data 事件数据； 不同事件下对应的数据类型不一致，详见下面说明
 *          事件类型            数据类型           示例
 *      SESSION_VOICE_TTS       char*        https://......
 * @param size 事件数据长度
 * @param usr 用户参数
 */
typedef void (*sessions_event_cb_t)(sessions_event_e evt, void *data, uint32_t size, void *usr);

int sessions_core_init(lsc_conn_t *conn);

session_t *session_create(void);
/**
 * @brief 添加事件回调函数
 *
 * @param cb 回调函数
 * @param evt 监听的事件，参考@see sessions_event_e
 * @param usr 用户参数
 *
 * @retval 0： 成功
 */
int session_add_evt_callback(session_t *hdl, sessions_event_cb_t cb, sessions_event_e evt, void *usr);

/**
 * @brief 参数设置
 *
 * @param cfg 配置参数
 *
 * @retval 0： 设置成功
 */
int session_set_config(session_t *hdl, session_params_t *cfg);
/**
 * @brief 获取设置参数
 *
 * @retval cfg： 配置参数
 */
int session_get_config(session_t *hdl, session_params_t *cfg);
/**
 * @brief 启动会话
 *
 * @param data 非必需，目前仅用于图像识别的start帧
 *
 * @retval 0： 成功
 */
int session_start(session_t *hdl, char *data);
/**
 * @brief 中止会话
 *
 * @retval 0： 成功
 */
int session_cancel(session_t *hdl);
/**
 * @brief 发送二进制数据
 *
 * @param data 数据
 * @param size 大小
 *
 * @retval 0： 成功
 */
int session_send_bin(session_t *hdl, const uint8_t *data, uint32_t size);
/**
 * @brief 主动中止上传数据
 * 一般用于客户端主动结束音频发送，云端会结束识别并进行对应的处理
 *
 * @retval 0： 成功
 */
int session_end(session_t *hdl);

int session_destroy(session_t *hdl);

int sessions_core_deinit(void);

session_t *sessions_core_find_by_sid(const char *sid);

#ifdef __cplusplus
}
#endif