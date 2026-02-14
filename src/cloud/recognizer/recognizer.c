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
		lisa_aiui_stop_send(s_recognizer->m_aiui);
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

    lisa_err_t err = lisa_aiui_cancel_send(s_recognizer->m_aiui);
    if (err != LISA_OK) {
        LISA_LOGE(TAG, "aiui send cancel failed");
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
		return;
	}
	s_drop_frame_count++;
	if(s_drop_frame_count < LS_DROP_AUDIO_FRAME_MAX) {
		return;
	}
	// TODO 过滤音频
	lisa_aiui_send_audio(handle->m_aiui, audio, len);
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
	LISA_LOGD(TAG, "cloud recognizer start");

    // if (lisa_aiui_get_interactive_mode() == INTER_CONTINUE) {
    //     ap2cp_algo_set_esr_timeout(LS_CLOUD_TTS_TIMEOUT);
    // }

	// 开始新一轮交互, 前面有交互未停止
	if (handle->m_state != IDLE) {
		// 前面正在录音
		if (handle->m_state == RECORD) {
			// 取消上次的录音
			handle->m_enable_audio = false;
			lisa_aiui_stop_send(handle->m_aiui);
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

		lisa_aiui_cancel_send(handle->m_aiui);

	}
	// 获取焦点
	listen_audiomgr_acquire_channel(handle->m_audio_mgr, AIP);
}

void recongizer_play_audio_id(uint8_t id)
{
	listen_shortplayer_play(s_recognizer->m_short_player, id);
}

static void _focus_state(focus_state_e focus_state, channel_type_e by_which)
{
	LISA_LOGD(TAG, "AIP focus state %d, by_which %d", focus_state, by_which);
	if (focus_state == FOREGROUND) {
		// Start Cloud Interactive

		s_recognizer->m_has_audio_focus = true;
		s_recognizer->m_state = RECORD;

		if (app_cloud_is_connected()) {
			int mode = lisa_aiui_get_interactive_mode();
			if (mode == INTER_ONESHOT || mode == INTER_CONTINUE) {
				listen_shortplayer_play(s_recognizer->m_short_player, 0);
			}
			s_drop_frame_count = 0;
			s_recognizer->m_enable_audio = true;
			// evs_uuid_generate_string(s_recognizer->m_sid);
			// extern void haoxueduo_role_start_text_play();
			// haoxueduo_role_start_text_play();
			// extern const char *haoxueduo_speaker_name_get();
			// const char *speaker_name = haoxueduo_speaker_name_get();
			// extern uint32_t haoxueduo_speaker_id_get();
			// uint32_t speaker_id = haoxueduo_speaker_id_get();
			extern int lisa_aiui_start_frame_send_audio(lisa_aiui_t *handle);
			if (LISA_OK != lisa_aiui_start_frame_send_audio(s_recognizer->m_aiui)) {
				// s_recognizer->m_aiui->aiui_ws_disconnect();
				LISA_LOGE(TAG, "audio session start failed.");
			}
			else {
				// 启动 ASR 定时器
				lisa_timer_start(s_recognizer->m_asr_timer);
			}
		} else {
			LISA_LOGW(TAG, "listen client is not connected");
			// listen_shortplayer_play(s_recognizer->m_short_player, 0);
		}
	} else if (focus_state == BACKGROUND) {
		listen_shortplayer_stop(s_recognizer->m_short_player);
	} else if (focus_state == NONE) {
		listen_shortplayer_stop(s_recognizer->m_short_player);
		// 无焦点
		s_recognizer->m_has_audio_focus = false;
		// 停止发送音频
        if (lisa_aiui_get_interactive_mode() == INTER_ONESHOT) {
            if (s_recognizer->m_state == RECORD) {
                lisa_aiui_stop_send(s_recognizer->m_aiui);
            }
            s_recognizer->m_enable_audio = false;
            s_recognizer->m_state = IDLE;
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
	if (handle->m_state == IDLE) {  // 已经释放了焦点
		return;
	} else if (handle->m_state == RECORD) {  // 收到ASR结果后就停止录音
		LISA_LOGD(TAG, "Stop cloud record");
		handle->m_enable_audio = false;
		lisa_aiui_stop_send(s_recognizer->m_aiui);
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
	// 状态调整为空
	handle->m_state = RECORD;

	handle->m_enable_audio = true;
	lisa_aiui_start_send_record(s_recognizer->m_aiui);
}

void recognizer_recognize_end(recognizer_t *handle)
{
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
	lisa_timer_stop(handle->m_asr_timer);
	// // 收到NLP结果, 停止 NLP 定时器
	// lisa_timer_stop(handle->m_nlp_timer);
	// 终止交互, 释放焦点
	if (handle->m_has_audio_focus) {
		listen_audiomgr_release_channel(handle->m_audio_mgr, AIP);
	}

	if (g_auto_stop_record_on_tts_end) {
		// 停止发送音频
		if (handle->m_state == RECORD) {
			handle->m_enable_audio = false;
			lisa_aiui_stop_send(handle->m_aiui);
		}
		// 状态调整为 IDLE
		handle->m_state = IDLE;
	}
}

void recognizer_manual_stop(void)
{
	if (s_recognizer == NULL) {
		LISA_LOGW(TAG, "Recognizer is not initialized");
		return;
	}

	LISA_LOGW(TAG, "recognizer manual stop");
	lisa_aiui_end_frame_send(s_recognizer->m_aiui);
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
    lisa_aiui_cancel_send(s_recognizer->m_aiui);
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
		lisa_aiui_cancel_send(s_recognizer->m_aiui);
	}
    recognizer_stop_record(s_recognizer);
    recognizer_recognize_end(s_recognizer);
}