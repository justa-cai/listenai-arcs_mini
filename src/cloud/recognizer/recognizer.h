#ifndef __LISTENAI_RECOGNIZER_H__
#define __LISTENAI_RECOGNIZER_H__

#include "stdbool.h"
#include "lisa_timer.h"
#include "short_player.h"
#include "tts_player.h"
#include "listen_audiomgr.h"
#include "lisa_aiui.h"

// 等待 ASR 结果超时时间
#define LS_CLOUD_ASR_TIMEOUT 15000
// 等待 NLP 结果超时时间
#define LS_CLOUD_NLP_TIMEOUT 10000

#define LS_CLOUD_TTS_TIMEOUT (30000)

typedef enum {
	IDLE,
	THINKING,
	RECORD,
} evs_recognizer_state_e;

#define LS_SESSION_ID_LEN (37)
typedef struct recognizer_s {
	// 唤醒播放器句柄
	short_player_t *m_short_player;
	// TTS Player
	tts_player_t *m_tts_player;
	// 交互状态
	evs_recognizer_state_e m_state;
	// 焦点管理句柄
	listen_audiomgr_t *m_audio_mgr;
	// 是否允许往云端送音频
	bool m_enable_audio;
	// 识别是否获取了焦点
	bool m_has_audio_focus;
	// 一次交互识别超时时间
	lisa_timer_t *m_asr_timer;
	// 一次交互NLP超时时间
	lisa_timer_t *m_nlp_timer;
	lisa_aiui_t *m_aiui;
	char m_sid[LS_SESSION_ID_LEN];
} recognizer_t;

recognizer_t *recognizer_create(short_player_t *short_player,
		tts_player_t *tts_player, listen_audiomgr_t *audio_mgr, lisa_aiui_t *aiui);

/**
 * @brief 	写入识别路音频, 录音持续调用
 * @param  	handle		识别句柄
 * @param  	audio		音频数据
 * @param  	len			数据长度
 */
void recognizer_write_audio(recognizer_t *handle, const char *audio, int len);

/**
 * @brief 	开始在线交互
 * @param  	handle		识别句柄
 */
void recognizer_recognize(recognizer_t *handle);

/**
 * @brief 	停止往云端送音频, 收到ASR结果后即可停止
 * @param  	handle		识别句柄
 */
void recognizer_stop_record(recognizer_t *handle);

/**
 * @brief 	重新往云端送音频, 收到ASR结果后即可停止
 * @param  	handle		识别句柄
 */
void recognizer_recognize_restart(recognizer_t *handle);

/**
 * @brief 	终止在线交互
 * @param  	handle		识别句柄
 */
void recognizer_recognize_end(recognizer_t *handle);

/**
 * @brief 	全双工单次识别结束
 * @param  	handle		识别句柄
 */
void recognizer_recognize_once_end(recognizer_t *handle);

void recognizer_record_suspend(void);
void recognizer_record_resume(void);

/**
 * @brief 	设置 TTS 结束后是否自动停止录音
 * @param  	enable		true: 自动停止(默认), false: 保持录音
 */
void recognizer_set_auto_stop_record(bool enable);

/**
 * @brief 	获取 TTS 结束后是否自动停止录音的设置
 * @return 	true: 自动停止, false: 保持录音
 */
bool recognizer_get_auto_stop_record(void);

#endif
