#define TAG "recognizer"

#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "lisa_err.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_time.h"
#include "evs_utils.h"
#include "lisa_timer.h"
#include "recognizer.h"
#include "app_tone.h"
#include "aiui_cfg.h"
#include "evs_uuid.h"
#include "app_cloud.h"
#include "assistant_controller.h"
#ifdef MY_CLOUD
#include "jk_cloud.h"
#endif

static recognizer_t *s_recognizer = NULL;

static bool g_auto_stop_record_on_tts_end = false;

void recognizer_set_auto_stop_record(bool enable)
{
	g_auto_stop_record_on_tts_end = enable;
	LISA_LOGI(TAG, "auto_stop_record_on_tts_end set to: %d", enable);
}

bool recognizer_get_auto_stop_record(void)
{
	return g_auto_stop_record_on_tts_end;
}

// AP 每次过来 16ms 数据, 过滤 52 帧
#define LS_DROP_AUDIO_FRAME_MAX (52)

static uint32_t s_drop_frame_count = 0;

static void _focus_state(focus_state_e focus_state, channel_type_e by_which);

static channel_callback_cb s_channel = {
		.on_focus_state = _focus_state,
		.m_channel_type = AIP,
};

/**
 * 停止交互
 */
static int _handle_stop_interactive(void *arg)
{
	LISA_LOGD(TAG, "Stop interactive by \"%s\"!!!", (char *)arg);

	if (s_recognizer->m_state == RECORD) {
		// 停止交互
		if (s_recognizer->m_aiui != NULL) {
			lisa_aiui_stop_send(s_recognizer->m_aiui);
		}
	}
	s_recognizer->m_enable_audio = false;
	s_recognizer->m_state = IDLE;

	LISA_LOGD(TAG, "stop all timer, release focus");
	// 停止 ASR 定时器
	lisa_timer_stop(s_recognizer->m_asr_timer);
	// // 停止 NLP 定时器
	// lisa_timer_stop(s_recognizer->m_nlp_timer);
	// 释放焦点
	if (s_recognizer->m_has_audio_focus) {
		listen_audiomgr_release_channel(s_recognizer->m_audio_mgr, AIP);
	}

	// Only call lisa_aiui_cancel_send if m_aiui is valid
	if (s_recognizer->m_aiui != NULL) {
		lisa_err_t err = lisa_aiui_cancel_send(s_recognizer->m_aiui);
		if (err != LISA_OK) {
			LISA_LOGE(TAG, "aiui send cancel failed");
		}
	}

	// play timeout tip
	char *url = app_tone_get_url(62);
	int url_len = strlen(url);
	audio_out_t audio = {0};
	memcpy(audio.m_url, url, url_len);
	audio.m_url[url_len] = '\0';
	s_recognizer->m_tts_player->on_directive(s_recognizer->m_tts_player, &audio, true);
	return 0;
}

// 在线交互 NLP 超时
static void __nlp_timeout(void *arg)
{
	evs_handler_post_runnable(_handle_stop_interactive, "NLP Timeout");
}

// 在线交互 ASR 超时
static void __asr_timeout(void *arg)
{
	lisa_timer_stop(s_recognizer->m_asr_timer);
	// evs_handler_post_runnable(_handle_stop_interactive, "ASR Timeout");
}

recognizer_t *recognizer_create(short_player_t *short_player,
		tts_player_t *tts_player, listen_audiomgr_t *audio_mgr, lisa_aiui_t *aiui)
{
	recognizer_t *handle =
			(recognizer_t *)lisa_mem_alloc(sizeof(recognizer_t));
	handle->m_audio_mgr = audio_mgr;
	handle->m_state = IDLE;
	handle->m_enable_audio = false;
	handle->m_has_audio_focus = false;
	handle->m_asr_timer = lisa_timer_create(LS_CLOUD_ASR_TIMEOUT, __asr_timeout, NULL);
	// handle->m_nlp_timer = lisa_timer_create(LS_CLOUD_NLP_TIMEOUT, __nlp_timeout, NULL);
	handle->m_short_player = short_player;
	handle->m_tts_player = tts_player;
	listen_audiomgr_add_channel_callback(handle->m_audio_mgr, &s_channel);
	handle->m_aiui = aiui;

	s_recognizer = handle;
	return handle;
}

void recognizer_write_audio(recognizer_t *handle, const char *audio, int len)
{
	if (!handle->m_enable_audio) {
		static int log_count = 0;
		if (log_count++ < 3) {
			LISA_LOGD(TAG, "recognizer_write_audio: m_enable_audio=false");
		}
		return;
	}
	s_drop_frame_count++;
	if(s_drop_frame_count < LS_DROP_AUDIO_FRAME_MAX) {
		// Log first frame and when approaching threshold
		if (s_drop_frame_count == 1) {
			LISA_LOGI(TAG, "Dropping first %d audio frames (dropping frame %d/%d)",
			          LS_DROP_AUDIO_FRAME_MAX, s_drop_frame_count, LS_DROP_AUDIO_FRAME_MAX);
		} else if (s_drop_frame_count == LS_DROP_AUDIO_FRAME_MAX - 1) {
			LISA_LOGI(TAG, "About to send first audio frame (drop count: %d/%d)",
			          s_drop_frame_count, LS_DROP_AUDIO_FRAME_MAX);
		}
		return;
	}
	if (s_drop_frame_count == LS_DROP_AUDIO_FRAME_MAX) {
		LISA_LOGI(TAG, "First audio frame sending (drop threshold reached: %d/%d), len: %d",
		          s_drop_frame_count, LS_DROP_AUDIO_FRAME_MAX, len);
	} else {
		LISA_LOGI(TAG, "send audio len: %d", len);
	}
#ifdef MY_CLOUD
	jk_cloud_audio(jk_cloud_get_instance(), audio, len);
#else
	lisa_aiui_send_audio(handle->m_aiui, audio, len);
#endif
}

void recognizer_stop_audio()
{
    if (s_recognizer == NULL || !s_recognizer->m_enable_audio) {
        return;
    }
    s_recognizer->m_enable_audio = false;

    LISA_LOGI(TAG, "cloud recognizer stop audio");
}

void recognizer_start_audio()
{
    if (s_recognizer == NULL || s_recognizer->m_enable_audio) {
        return;
    }
    s_recognizer->m_enable_audio = true;

    LISA_LOGI(TAG, "cloud recognizer start audio");
}

void recognizer_recognize(recognizer_t *handle)
{
	LISA_LOGI(TAG, "recognizer_recognize: current_state=%d (IDLE=%d,RECORD=%d)",
	          handle->m_state, IDLE, RECORD);

    // if (lisa_aiui_get_interactive_mode() == INTER_CONTINUE) {
    //     ap2cp_algo_set_esr_timeout(LS_CLOUD_TTS_TIMEOUT);
    // }

	// 开始新一轮交互, 前面有交互未停止
	if (handle->m_state != IDLE) {
		// 前面正在录音
		if (handle->m_state == RECORD) {
			// 取消上次的录音
			LISA_LOGI(TAG, "recognizer_recognize: stopping previous RECORD session");
			handle->m_enable_audio = false;
			// Only call lisa_aiui_stop_send if m_aiui is valid
			if (handle->m_aiui != NULL) {
				lisa_aiui_stop_send(handle->m_aiui);
			}
		}
		// 停止 ASR 定时器
		lisa_timer_stop(handle->m_asr_timer);
		// // 停止 NLP 定时器
		// lisa_timer_stop(handle->m_nlp_timer);

		handle->m_state = IDLE;

        if (lisa_aiui_get_interactive_mode() == INTER_CONTINUE) {
			extern char old_fid[37];
			extern char new_fid[37];
			memset(old_fid, 0, sizeof(old_fid));
			strcpy(old_fid, new_fid);
        }

		// Only call lisa_aiui_cancel_send if m_aiui is valid
		if (handle->m_aiui != NULL) {
			lisa_aiui_cancel_send(handle->m_aiui);
		}

	}
	// 获取焦点
	LISA_LOGI(TAG, "recognizer_recognize: acquiring AIP channel focus");
	listen_audiomgr_acquire_channel(handle->m_audio_mgr, AIP);
}

void recongizer_play_audio_id(uint8_t id)
{
	if (s_recognizer && s_recognizer->m_short_player) {
		listen_shortplayer_play(s_recognizer->m_short_player, id);
	}
}

static void _focus_state(focus_state_e focus_state, channel_type_e by_which)
{
	LISA_LOGI(TAG, "AIP focus state %d (FOREGROUND=%d,BACKGROUND=%d,NONE=%d), by_which %d",
	          focus_state, FOREGROUND, BACKGROUND, NONE, by_which);
	if (focus_state == FOREGROUND) {
		// Start Cloud Interactive

		s_recognizer->m_has_audio_focus = true;
		s_recognizer->m_state = RECORD;

		LISA_LOGI(TAG, "FOREGROUND: checking cloud connection...");
		if (app_cloud_is_connected()) {
			LISA_LOGI(TAG, "FOREGROUND: cloud connected, enabling audio, drop_frame_count=%u",
			          s_drop_frame_count);
			s_drop_frame_count = 0;
			s_recognizer->m_enable_audio = true;
			int mode = lisa_aiui_get_interactive_mode();
			LISA_LOGI(TAG, "FOREGROUND: interactive_mode=%d, m_enable_audio=%d",
			          mode, s_recognizer->m_enable_audio);
			if (mode == INTER_ONESHOT || mode == INTER_CONTINUE) {
				listen_shortplayer_play(s_recognizer->m_short_player, 0);
			}
			// evs_uuid_generate_string(s_recognizer->m_sid);
			// extern void haoxueduo_role_start_text_play();
			// haoxueduo_role_start_text_play();
			// extern const char *haoxueduo_speaker_name_get();
			// const char *speaker_name = haoxueduo_speaker_name_get();
			extern int lisa_aiui_start_frame_send_audio(lisa_aiui_t *handle);
#ifndef MY_CLOUD
			if (LISA_OK != lisa_aiui_start_frame_send_audio(s_recognizer->m_aiui)) {
				// s_recognizer->m_aiui->aiui_ws_disconnect();
				LISA_LOGE(TAG, "audio session start failed.");
			}
			else {
				// 启动 ASR 定时器
				lisa_timer_start(s_recognizer->m_asr_timer);
			}
#else
			lisa_timer_start(s_recognizer->m_asr_timer);
#endif
		} else {
			LISA_LOGW(TAG, "FOREGROUND: listen client is not connected, m_enable_audio=%d",
			          s_recognizer->m_enable_audio);
			// listen_shortplayer_play(s_recognizer->m_short_player, 0);
		}
	} else if (focus_state == BACKGROUND) {
		LISA_LOGI(TAG, "BACKGROUND: stopping shortplayer");
		listen_shortplayer_stop(s_recognizer->m_short_player);
	} else if (focus_state == NONE) {
		LISA_LOGI(TAG, "NONE: losing focus, m_enable_audio=%d", s_recognizer->m_enable_audio);
		listen_shortplayer_stop(s_recognizer->m_short_player);
		// 无焦点
		s_recognizer->m_has_audio_focus = false;
		// 停止发送音频
        if (lisa_aiui_get_interactive_mode() == INTER_ONESHOT) {
            if (s_recognizer->m_state == RECORD) {
				// Only call lisa_aiui_stop_send if m_aiui is valid
				if (s_recognizer->m_aiui != NULL) {
					lisa_aiui_stop_send(s_recognizer->m_aiui);
				}
            }
            s_recognizer->m_enable_audio = false;
            s_recognizer->m_state = IDLE;
            LISA_LOGI(TAG, "NONE: INTER_ONESHOT mode, stopped recording");
        }
		// 停止 ASR 定时器
		lisa_timer_stop(s_recognizer->m_asr_timer);
		// // 停止 NLP 定时器
		// lisa_timer_stop(s_recognizer->m_nlp_timer);
	}
}

void recognizer_record_suspend(void)
{
	s_recognizer->m_enable_audio = false;
}

void recognizer_record_resume(void)
{
	s_recognizer->m_enable_audio = true;
}

void recognizer_stop_record(recognizer_t *handle)
{
	if (!handle) {
		LISA_LOGW(TAG, "recognizer_stop_record: handle is NULL");
		return;
	}
	if (handle->m_state == IDLE) {  // 已经释放了焦点
		return;
	} else if (handle->m_state == RECORD) {  // 收到ASR结果后就停止录音
		LISA_LOGD(TAG, "Stop cloud record");
		handle->m_enable_audio = false;
		// Only call lisa_aiui_stop_send if m_aiui is valid
		if (s_recognizer->m_aiui != NULL) {
			lisa_aiui_stop_send(s_recognizer->m_aiui);
		}
	}
	// 状态调整为Thinking
	handle->m_state = THINKING;
	// 停止 ASR Timer
	// lisa_timer_stop(handle->m_asr_timer);
	// // 启动 NLP Timer
	// lisa_timer_start(handle->m_nlp_timer);
}

void recognizer_recognize_restart(recognizer_t *handle)
{
	if (!handle) {
		LISA_LOGW(TAG, "recognizer_recognize_restart: handle is NULL");
		return;
	}
	// 状态调整为空
	handle->m_state = RECORD;

	handle->m_enable_audio = true;
	// Only call lisa_aiui_start_send_record if m_aiui is valid
	if (s_recognizer->m_aiui != NULL) {
		lisa_aiui_start_send_record(s_recognizer->m_aiui);
	}
}

void recognizer_recognize_end(recognizer_t *handle)
{
	if (!handle) {
		LISA_LOGW(TAG, "recognizer_recognize_end: handle is NULL");
		return;
	}
	lisa_timer_stop(handle->m_asr_timer);
	// // 收到NLP结果, 停止 NLP 定时器
	// lisa_timer_stop(handle->m_nlp_timer);
	// 终止交互, 释放焦点
	if (handle->m_has_audio_focus) {
		listen_audiomgr_release_channel(handle->m_audio_mgr, AIP);
	}

	// 状态调整为空
	handle->m_state = IDLE;
}

void recognizer_recognize_once_end(recognizer_t *handle)
{
	LISA_LOGI(TAG, "recognizer_recognize_once_end: auto_stop_record=%d, state=%d, has_focus=%d",
	          g_auto_stop_record_on_tts_end, handle->m_state, handle->m_has_audio_focus);

	lisa_timer_stop(handle->m_asr_timer);
	// // 收到NLP结果, 停止 NLP 定时器
	// lisa_timer_stop(handle->m_nlp_timer);
	// 终止交互, 释放焦点
	if (handle->m_has_audio_focus) {
		LISA_LOGI(TAG, "Releasing AIP channel");
		listen_audiomgr_release_channel(handle->m_audio_mgr, AIP);
	}

	if (g_auto_stop_record_on_tts_end) {
		// 停止发送音频
		if (handle->m_state == RECORD) {
			LISA_LOGI(TAG, "Auto stop: stopping audio send, state RECORD->IDLE");
			handle->m_enable_audio = false;
			// Only call lisa_aiui_stop_send if m_aiui is valid
			if (handle->m_aiui != NULL) {
				lisa_aiui_stop_send(handle->m_aiui);
			}
		} else {
			LISA_LOGI(TAG, "Auto stop: state=%d, only set to IDLE", handle->m_state);
		}
		// 状态调整为 IDLE
		handle->m_state = IDLE;
	} else {
		LISA_LOGI(TAG, "Auto stop disabled, keeping current state");
	}
}

void recognizer_manual_stop(void)
{
	if (s_recognizer == NULL) {
		LISA_LOGW(TAG, "Recognizer is not initialized");
		return;
	}

	LISA_LOGW(TAG, "recognizer manual stop");
	// Only call lisa_aiui_end_frame_send if m_aiui is valid
	if (s_recognizer->m_aiui != NULL) {
		lisa_aiui_end_frame_send(s_recognizer->m_aiui);
	}
	recognizer_stop_record(s_recognizer);
	recognizer_recognize_end(s_recognizer);
}

void recognizer_stop(void)
{
    if (s_recognizer == NULL) {
        LISA_LOGW(TAG, "Recognizer is not initialized");
        return;
    }

    LISA_LOGW(TAG, "recognizer stop");
	// Only call lisa_aiui_cancel_send if m_aiui is valid
	if (s_recognizer->m_aiui != NULL) {
		lisa_aiui_cancel_send(s_recognizer->m_aiui);
	}
    recognizer_stop_record(s_recognizer);
    recognizer_recognize_end(s_recognizer);
}

void recognizer_stop_haoxueduo(void)
{
    if (s_recognizer == NULL) {
        LISA_LOGW(TAG, "Recognizer is not initialized");
        return;
    }

	s_recognizer->m_tts_player->stop(s_recognizer->m_tts_player);

    LISA_LOGW(TAG, "recognizer stop");

	if (app_cloud_is_connected()) {
		// Only call lisa_aiui_cancel_send if m_aiui is valid
		if (s_recognizer->m_aiui != NULL) {
			lisa_aiui_cancel_send(s_recognizer->m_aiui);
		}
	}
    recognizer_stop_record(s_recognizer);
    recognizer_recognize_end(s_recognizer);
}