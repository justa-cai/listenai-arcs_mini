#define TAG "proc_mgr"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "app_cloud.h"
#include "proc_mgr.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_aiui.h"
#include "evs_utils.h"
#include "cJSON.h"
#include "audio_out.h"
#include "lisa_timer.h"
#include "aiui_cfg.h"
#include "app_client.h"
#include "recognizer.h"
#include "audio_player.h"
#include "listen_volume.h"
#include "tts_player.h"
#include "tone.h"
#include "sound_player.h"
#include "aiui_base64.h"
#include "app_tone.h"
#include "stdlib.h"
#include "assistant_controller.h"
#include "alarm_aiui.h"
#include "lisa_aiui_rid_man.h"
#include "mcp_integration.h"

typedef enum {
	NLP_RESULT_INIT,
	NLP_RESULT_MEDIA,
	NLP_RESULT_BROADCAST,
	NLP_RESULT_CONTROL,
	NLP_RESULT_UNSUPPORT,
	NLP_RESULT_ERROR,
} nlp_result_state;

static audio_out_t *s_audio_list = NULL;
static int s_audio_size = 0;
static nlp_result_state s_nlp_result = NLP_RESULT_INIT;
static lisa_timer_t *tts_timer = NULL;
char old_fid[37] = {0};
char new_fid[37] = {0};

static recognizer_t *s_rec = NULL;
static app_client_t *s_app_client = NULL;
static audioplayer_t *s_audio_player = NULL;
static tts_player_t *s_tts_player = NULL;
static sound_player_t *s_sound_player = NULL;
static lisa_aiui_t *s_aiui = NULL;
static lisa_semaphore_t *started_sema = NULL;

static void __tts_timeout(void *arg);
static int _proc_msg_continue(void *arg);
static void _on_tts_focus_state(focus_state_e state, channel_type_e by_which);

static char curr_tts_cid[32] = {0};

void started_reset(void)
{
    if (started_sema) {
		while (lisa_semaphore_take(started_sema, 0) == 0) {
		}
    }
}

int started_wait(int timeout)
{
    if (started_sema) {
        return lisa_semaphore_take(started_sema, timeout);
    }

    return 0;
}

void started_give(void)
{
    if (started_sema) {
        lisa_semaphore_give(started_sema);
    }
}

static tts_focus_callback_t s_focus_state_cb = {
		.on_focus_state = _on_tts_focus_state,
};

void app_proc_init(app_client_t *app_client, app_cloud_t *cloud)
{
	s_rec = cloud->m_rec;
	s_aiui = cloud->aiui;
	s_app_client = app_client;
	s_audio_player = app_client->audio_player;
	s_tts_player = app_client->tts_player;
	s_sound_player = app_client->sound_player;
	listen_ttsplayer_add_focus_callback(s_tts_player, &s_focus_state_cb);
	tts_timer = lisa_timer_create(LS_CLOUD_TTS_TIMEOUT, __tts_timeout, NULL);
	started_sema = lisa_semaphore_create(1);
}

void app_proc_msg(const char *msg, int len)
{
	LISA_LOGD(TAG, "app_proc_msg len %d\n", len);
	char *data = lisa_mem_calloc(1, len + 1);
	if (data == NULL) {
		LISA_LOGE(TAG, "alloc mem failed, drop data");
		return;
	}
	memcpy(data, msg, len);
	data[len] = '\0';
	evs_handler_post_runnable(_proc_msg_continue, (void *)data);
}

static int play_timeout_audio(void)
{
    listen_soundplayer_play(s_sound_player, TONE_ID_62, 0);
    return 0;
}

static int play_net_error_audio(void)
{
    listen_soundplayer_play(s_sound_player, TONE_ID_65, 0);
	return 0;
}

int play_start_config_net_audio(void)
{
    listen_soundplayer_play(s_sound_player, TONE_ID_70, 0);
    return 0;
}

int play_config_net_success_audio(void)
{
    listen_soundplayer_play(s_sound_player, TONE_ID_72, 0);
    return 0;
}

int play_factory_reset_audio(void)
{
    listen_soundplayer_play(s_sound_player, TONE_ID_103, 0);
    return 0;
}

int play_auth_failed_audio(void)
{
    listen_soundplayer_play(s_sound_player, TONE_ID_105, 0);
	return 0;
}

static void _release_audio_list()
{
	if (s_audio_list) lisa_mem_free(s_audio_list);
	s_audio_list = NULL;
	s_audio_size = 0;
}

static void _on_tts_focus_state(focus_state_e state, channel_type_e by_which)
{
	LISA_LOGD(TAG, "tts_focus_callback, state %d, by %d, audio size %d", state, by_which,
			s_audio_size);
	if (state == NONE) {
		if (by_which == TTS) {
			audioplayer_t *audioplayer = s_audio_player;
			if (s_audio_list) {
				audioplayer->on_directive(audioplayer, AUDIO_PLAY, s_audio_list, s_audio_size);
				_release_audio_list();
			}
            if (lisa_aiui_get_interactive_mode() == INTER_CONTINUE) {
                lisa_timer_start(tts_timer);
            }
		} else if (by_which == AIP) {  // TTS被唤醒打断, 不清除资源

		} else {  // tts被资源播放、闹钟打断，不播放歌曲，不反问
			_release_audio_list();
		}
	}
}

/**
 * 停止交互
 */
static int _handle_stop_interactive(void *arg)
{
	recognizer_stop_record(s_rec);
	recognizer_recognize_end(s_rec);
    lisa_err_t err = lisa_aiui_cancel_send(s_aiui);
    if (err != LISA_OK) {
        LISA_LOGE(TAG, "aiui send cancel failed");
    }
    play_timeout_audio();
	assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PRE_IDLE, NULL, 0);
	LISA_LOGW(TAG, "Interactive has end!!!");
    return 0;
}

// tts播放完成后超时
static void __tts_timeout(void *arg)
{
	lisa_timer_stop(tts_timer);
	// evs_handler_post_runnable(_handle_stop_interactive, "TTS Timeout");
}

static int _parser_audios(cJSON *root, audio_out_t **audio)
{
	if (!root) {
		return -1;
	}
	cJSON *service = cJSON_GetObjectItem(root, "service");
	if ((!service)||(strcmp(service->valuestring, "musicX") != 0)) return -1;
	cJSON *data = cJSON_GetObjectItem(root, "data");
	if (!data) return -1;
	cJSON *array = cJSON_GetObjectItem(data, "result");
	if (!array) return -1;
	const int size = cJSON_GetArraySize(array);
	if (size <= 0) return -1;

	audio_out_t *audios = (audio_out_t *)lisa_mem_calloc(size, sizeof(audio_out_t));
	if (!audios) return -1;

	int canplay_count = 0;
	int cantplay_count = 0;

	for (int i = 0, j = 0; i < size; i++) {
		cJSON *item = cJSON_GetArrayItem(array, i);
		cJSON *param = cJSON_GetObjectItem(item, "uni_url");
		if (param && param->valuestring) {
			strcpy(audios[canplay_count].m_url, param->valuestring);
		}
		// param = cJSON_GetObjectItem(item, "playable");
		// if (param && (param->valueint == 0)) {
		// 	cantplay_count++;
		// 	continue;
		// }
		param = cJSON_GetObjectItem(item, "allRate");
		if (param && param->valuestring) {
			strcpy(audios[canplay_count].m_all_rate, param->valuestring);
		}
		param = cJSON_GetObjectItem(item, "itemid");
		if (param) {
			strcpy(audios[canplay_count].mid, param->valuestring);
		}

		param = cJSON_GetObjectItem(item, "name");
		if (param) {
			int jj = 0;
			int ii = 0;
			for(;ii<sizeof(audios[canplay_count].m_name) -1;jj++){
				if((param->valuestring[jj]=='[')
					|| (param->valuestring[jj]==']')
					|| (param->valuestring[jj]=='\"')){
					continue;
				}
				audios[canplay_count].m_name[ii++] = param->valuestring[jj];
				if(param->valuestring[jj]=='\0'){
					break;
				}
			}
			audios[canplay_count].m_name[ii] = '\0';
		}
		

		canplay_count++;
	}

	LISA_LOGW(TAG, "all audio is not playable,cantplay_count:%d,size:%d",cantplay_count,size);
	// if (cantplay_count == size) {
	// 	lisa_mem_free(audios);
	// 	return 0;
	// }

	*audio = audios;
	return canplay_count;
}

#define VOL_TTS_FORMAT "当前音量为百分之%d"

int weather_aiui_intent_process(cJSON *intent_root)
{
	cJSON *data = cJSON_GetObjectItem(intent_root, "data");
	if (!data) return -1;
	cJSON *array = cJSON_GetObjectItem(data, "result");
	if (!array) return -1;
	const int size = cJSON_GetArraySize(array);
	if (size <= 0) return -1;

	cJSON *item = cJSON_GetArrayItem(array, 0);
	char *json_str = cJSON_PrintUnformatted(item); // 格式化成字符串
	LISA_LOGI(TAG, "weather_aiui_intent_process, text: %s", json_str);
	assist_controller_trigger_event(CONTROLLER_EVENT_STATE_WEATHER_UPDATE, json_str, strlen(json_str) + 1);
    free(json_str);

	return 0;
}


static int _parser_intent(cJSON *root)
{
	#define MUSIC_DELAY  2000//延时，防止tts被打断

	if (!root) return -1;
	cJSON *rc_item = cJSON_GetObjectItem(root, "rc");
	if (!rc_item || !cJSON_IsNumber(rc_item)) return -1;
	int rc_code = rc_item->valueint;
	if (rc_code != 0) return -1;

	cJSON *service = cJSON_GetObjectItem(root, "service");
	if (service != NULL) {

		if (strcmp(service->valuestring, "weather") == 0) {
			/* weather msg */
			LISA_LOGI(TAG, "aiui weather intent match");
			weather_aiui_intent_process(root);
			return 0;
		}

		if (strcmp(service->valuestring, "scheduleX") == 0) {
			/* alarm msg */
			LISA_LOGI(TAG, "aiui alarm intent match");
			// alarm_aiui_intent_process(root);
			return 0;
		}
	}
	LISA_LOGW(TAG, "[%s %d]", __FUNCTION__,__LINE__);
	cJSON *service_pkg_item = cJSON_GetObjectItem(root, "service_pkg");
	if (service_pkg_item && (strcmp(service_pkg_item->valuestring, "broadcast") == 0)) {
		return 2;
	}


	cJSON *semantic = cJSON_GetObjectItem(root, "semantic");
	if (!semantic) return -1;
	const int size = cJSON_GetArraySize(semantic);
	if (size <= 0) return -1;
	cJSON *sema_item = cJSON_GetArrayItem(semantic, 0);
	if (!sema_item) return -1;
	cJSON *intent = cJSON_GetObjectItem(sema_item, "intent");
	if (!intent || !cJSON_IsString(intent)) return -1;

	if (strcmp(intent->valuestring, "PAUSE") == 0) {  // 暂停
		listen_audioplayer_puse(s_audio_player);
		recognizer_stop_record(s_rec);
		recognizer_recognize_end(s_rec);
		assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PRE_IDLE, NULL, 0);
		return 1;
	} else if (strcmp(intent->valuestring, "INSTRUCTION") == 0) {
		cJSON *slots = cJSON_GetObjectItem(sema_item, "slots");
		if (!slots) return -1;

		cJSON *slot = cJSON_GetArrayItem(slots, 0);
		if (!slot) return -1;

		cJSON *value = cJSON_GetObjectItem(slot, "value");
		if (!value) return -1;

		// cJSON *service = cJSON_GetObjectItem(root, "service");
		if (!service || (strcmp(service->valuestring, "musicX") != 0)) {
			return -1;
		}
		if (strcmp(value->valuestring, "close") == 0) {
			listen_audioplayer_puse(s_audio_player);
			recognizer_stop_record(s_rec);
			recognizer_recognize_end(s_rec);
			assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PRE_IDLE, NULL, 0);
		} else {
			return -1;
		}
		return 1;
	} else if (strcmp(intent->valuestring, "REPLAY") == 0) {  // 继续
		lisa_thread_mdelay(MUSIC_DELAY);
		s_audio_player->replay(s_audio_player);
		return 1;
	} else if (strcmp(intent->valuestring, "RESUME_PLAY") == 0) {  // 继续
		lisa_thread_mdelay(MUSIC_DELAY);
		s_audio_player->resumeByVoice(s_audio_player);
		return 1;
	} else if (strcmp(intent->valuestring, "CHOOSE_NEXT") == 0) {  // 下一首
		lisa_thread_mdelay(MUSIC_DELAY);
		s_audio_player->next(s_audio_player);
		return 1;
	} else if (strcmp(intent->valuestring, "CHOOSE_PREVIOUS") == 0) {  // 上一首
		lisa_thread_mdelay(MUSIC_DELAY);
		s_audio_player->prev(s_audio_player);
		return 1;
	} else if (strcmp(intent->valuestring, "VOLUME_MINUS") == 0) {  // 声音小
		listen_vol_adjust(-10);
		return 0;
	} else if (strcmp(intent->valuestring, "VOLUME_PLUS") == 0) {  // 声音大
		listen_vol_adjust(10);
		return 0;
	} else if (strcmp(intent->valuestring, "MUTE") == 0) {  // 静音
		listen_vol_mute();
		return 0;
	} else if (strcmp(intent->valuestring, "UNMUTE") == 0) {  // 取消静音
		listen_vol_cancel_mute();
		return 0;
	} else if (strcmp(intent->valuestring, "VOLUME_MAX") == 0) {  // 音量最大
		listen_set_volume(100);
		return 0;
	} else if (strcmp(intent->valuestring, "VOLUME_MIN") == 0) {  // 音量最小
		listen_set_volume(0);
		return 0;
	} else if (strcmp(intent->valuestring, "VOLUME_MID") == 0) {  // 音量中等
		listen_set_volume(50);
		return 0;
	} else if (strcmp(intent->valuestring, "VOLUME_QUERY") == 0) {
		char *buffer = lisa_mem_alloc(strlen(VOL_TTS_FORMAT) + 10);
		sprintf(buffer, VOL_TTS_FORMAT, listen_get_volume());
		app_cloud_tts(buffer);
		lisa_mem_free(buffer);
		return 0;
	} else if (strcmp(intent->valuestring, "VOLUME_SET") == 0) {  // 设置音量值
		cJSON *slots = cJSON_GetObjectItem(sema_item, "slots");
		const int slot_size = cJSON_GetArraySize(slots);
		if (slot_size <= 0) return 0;
		cJSON *vol_item = cJSON_GetArrayItem(slots, 0);
		if (!vol_item) return 0;
		cJSON *vol_value = cJSON_GetObjectItem(vol_item, "value");
		if (!vol_value || !cJSON_IsString(vol_value)) return 0;
		int vol = atoi(vol_value->valuestring);
		listen_set_volume(vol);
		return 0;
	} else {
		return -1;
	}
}

bool _ignore_msg(cJSON *root, cJSON *action)
{
	cJSON *tag = cJSON_GetObjectItem(root, "tag");
	if (tag) {
		if (strcmp(tag->valuestring, s_rec->m_sid) != 0) {
			LISA_LOGD(TAG, "old sid %s local sid %s, not equal dorp this msg", tag->valuestring,
					s_rec->m_sid);
			return true;
		}
	}


	if (lisa_aiui_get_interactive_mode() == INTER_CONTINUE) {
		cJSON *fid = cJSON_GetObjectItem(root, "fid");
		if (fid) {
			if (strcmp(action->valuestring, "started") == 0) {
				memset(new_fid, 0, sizeof(new_fid));
				strcpy(new_fid, fid->valuestring);
			}

			LISA_LOGD(TAG, "old: %s, fid: %s\r\n", old_fid, fid->valuestring);
			if (strcmp(old_fid, fid->valuestring) == 0) {
				LISA_LOGE(TAG, "warn: new fid %s ie equal to old fid %s, drop it\r\n", fid->valuestring,
						old_fid);
				return true;
			}
		}
	}
	return false;
}

static int _proc_msg_continue(void *arg)
{
	if (!arg) return 0;

	// LISA_LOGI(TAG, "aiui msg: %s", (char *)arg);
	cJSON *root = cJSON_Parse((char *)arg);
	if (!root) {
		LISA_LOGD(TAG, "%s@%d parser failed", __FUNCTION__, __LINE__);
		lisa_mem_free(arg);
		return 0;
	}

	cJSON *action = cJSON_GetObjectItem(root, "action");
	if (!action || !cJSON_IsString(action)) {
		LISA_LOGD(TAG, "%s@%d parser failed", __FUNCTION__, __LINE__);
		goto PARSER_END;
	}

    cJSON *mcp_response = mcp_integration_process_message(root);
    if (mcp_response) {
        // 有 MCP 响应，发送并结束处理
        char *response_str = cJSON_Print(mcp_response);
        if (response_str) {
            // aiui_send_response(response_str);
			LISA_LOGI(TAG, "mcp response: %s\n", response_str);
			app_cloud_txt(response_str);
			// printf("mcp response: %s\n", response_str);
            cJSON_free(response_str);
        }
        cJSON_Delete(mcp_response);
        cJSON_Delete(root);
        lisa_mem_free(arg);
        return 0;  // MCP 消息已处理完毕
    }

	if (strcmp(action->valuestring, "started") == 0) {
		started_give();
	}

	if (_ignore_msg(root, action)) goto PARSER_END;

	if (strcmp(action->valuestring, "result") == 0) {
		cJSON *data = cJSON_GetObjectItem(root, "data");
		if (!data) goto PARSER_END;

		cJSON *nlp_origin = cJSON_GetObjectItem(data, "nlp_origin");
		if (nlp_origin && cJSON_IsString(nlp_origin) && strcmp(nlp_origin->valuestring, "emoji") == 0) {
			cJSON *_data = cJSON_GetObjectItem(data, "data");
			cJSON *emo_id = cJSON_GetObjectItem(_data, "emo_id");

			if (emo_id && cJSON_IsString(emo_id)) {
				LISA_LOGI(TAG, "emoji: %s", emo_id->valuestring);
				assist_controller_trigger_event(CONTROLLER_EVENT_STATE_ROLE_EMOJI_UPDATE, emo_id->valuestring, strlen(emo_id->valuestring)+1);
			}
		}

		if (nlp_origin && cJSON_IsString(nlp_origin) && strcmp(nlp_origin->valuestring, "image_generation") == 0) {
			LISA_LOGI(TAG, "Processing image_generation skill");
			
			// 检查是否有错误，如有错误则提前处理
			cJSON *_data = cJSON_GetObjectItem(data, "data");
			if (_data) {
				cJSON *result_array = cJSON_GetObjectItem(_data, "result");
				if (result_array && cJSON_IsArray(result_array)) {
					int result_count = cJSON_GetArraySize(result_array);
					
					// 快速检查是否有错误
					for (int i = 0; i < result_count; i++) {
						cJSON *result_item = cJSON_GetArrayItem(result_array, i);
						if (result_item) {
							cJSON *error = cJSON_GetObjectItem(result_item, "error");
							if (error) {
								cJSON *error_code = cJSON_GetObjectItem(error, "code");
								if (error_code && cJSON_IsString(error_code) && (strcmp(error_code->valuestring, "OutOfLimit") == 0)) {
									// 传递完整的 result_item JSON 对象，包含 error 和 url 信息
									char *result_item_json_str = cJSON_Print(result_item);
									if (result_item_json_str) {
										LISA_LOGI(TAG, "Sending OutOfLimit error with full result_item: %s", result_item_json_str);
										// 触发错误处理事件，传递完整的 result_item
										assist_controller_trigger_event(CONTROLLER_EVENT_STATE_OUTOF_LIMIT_ERROR, 
																		result_item_json_str, strlen(result_item_json_str) + 1);
										cJSON_free(result_item_json_str);
									}
									break;
								}
								

							}
						}
					}
				}
			}
			goto PARSER_END;
		}

		// 不处理reply_text
		// if (nlp_origin && cJSON_IsString(nlp_origin) && strcmp(nlp_origin->valuestring, "reply_text") == 0) {
		// 	cJSON *nlp = cJSON_GetObjectItem(data, "nlp");
		// 	if (nlp) {
		// 		LISA_LOGI(TAG, "nlp: %s", nlp->valuestring);
		// 		cJSON *stream_url = cJSON_GetObjectItem(nlp, "stream_url");
		// 		if (stream_url && cJSON_IsString(stream_url)) {
		// 			assist_controller_trigger_event(CONTROLLER_EVENT_STATE_REPLY_TEXT_UPDATE, stream_url->valuestring, strlen(stream_url->valuestring)+1);
		// 		}
		// 	}
		// }

		cJSON *theme = cJSON_GetObjectItem(data, "theme");
		if (theme) {
			// 解析主题配置，提取待机文本
			cJSON *frontend = cJSON_GetObjectItem(theme, "frontend");
			if (frontend) {
				cJSON *banner = cJSON_GetObjectItem(frontend, "banner");
				if (banner) {
					cJSON *resources = cJSON_GetObjectItem(banner, "resources");
					if (resources && cJSON_IsArray(resources)) {
						// 将整个 banner 对象转换为字符串，通过事件总线发送
						char *banner_str = cJSON_Print(banner);
						if (banner_str) {
							LISA_LOGI(TAG, "Sending standby texts update: %s", banner_str);
							assist_controller_trigger_event(CONTROLLER_EVENT_STATE_STANDBY_TEXTS_UPDATE, 
															banner_str, strlen(banner_str) + 1);
							cJSON_free(banner_str);
						}
					}
				}
			}
		}

		// 解析 mp_guide 配置，用于设备配置页面
		cJSON *mp_guide = cJSON_GetObjectItem(data, "mp_guide");
		if (mp_guide) {
			cJSON *role_config = cJSON_GetObjectItem(mp_guide, "role_config");
			if (role_config) {
				// 将整个 role_config 对象转换为字符串，通过事件总线发送
				char *role_config_str = cJSON_Print(role_config);
				if (role_config_str) {
					LISA_LOGI(TAG, "Sending device config update: %s", role_config_str);
					assist_controller_trigger_event(CONTROLLER_EVENT_STATE_DEVICE_CONFIG_UPDATE, 
													role_config_str, strlen(role_config_str) + 1);
					cJSON_free(role_config_str);
				}
			}
		}

		cJSON *vad_sub = cJSON_GetObjectItem(data, "sub");
		if (vad_sub && cJSON_IsString(vad_sub) && (strcmp(vad_sub->valuestring, "vad") == 0)) {
			if (lisa_aiui_get_interactive_mode() == INTER_ONESHOT) {
				// assist_controller_trigger_event(CONTROLLER_EVENT_STATE_VAD_END, NULL, 0);
				recognizer_stop_record(s_rec);
			}
			// assist_controller_trigger_event(CONTROLLER_EVENT_STATE_VAD_END, NULL, 0);
			goto PARSER_END;
		}
		cJSON *is_last = cJSON_GetObjectItem(data, "is_last");
		cJSON *sub = cJSON_GetObjectItem(data, "sub");
		cJSON *text = cJSON_GetObjectItem(data, "text");
		if (is_last && cJSON_IsBool(is_last) && is_last->valueint) {
			if (lisa_aiui_get_interactive_mode() == INTER_ONESHOT) {
				recognizer_recognize_end(s_rec);
			} else {
				recognizer_recognize_once_end(s_rec);
			}
			if (!sub || !cJSON_IsString(sub)) {
				LISA_LOGE(TAG, "sub is not string");
				goto PARSER_END;
			}
			if (strcmp(sub->valuestring, "tts") == 0) {
				strcpy(curr_tts_cid, cJSON_GetObjectItem(root, "cid")->valuestring);
				LISA_LOGI(TAG, "curr_tts_cid: %s", curr_tts_cid);
				cJSON *content = cJSON_GetObjectItem(data, "content");
				if (content && cJSON_IsString(content)) {
					const int content_len = strlen(content->valuestring);
					audio_out_t audio = {0};
					if (content_len > 10) {
						int decLen = content_len / 4.0 * 3 + 8;
						char *url = (char *)lisa_mem_calloc(1, sizeof(char) * decLen);
						int out_len;
						int ret = aiui_base64_decode(
								content->valuestring, content_len, url, &out_len);
						if (ret == 0) {
							memcpy(audio.m_url, url, out_len);
						} else {
							LISA_LOGE(TAG, "decode url failed\n");
							char *url_tone = app_tone_get_url(61);
							out_len = strlen(url_tone);
							memcpy(audio.m_url, url_tone, out_len);
						}
						audio.m_url[out_len] = '\0';
						audio.throw_time = 300;
						if (lisa_aiui_get_interactive_mode() == INTER_CONTINUE) {
							lisa_timer_stop(tts_timer);
							// ap2cp_algo_set_esr_timeout(LS_CLOUD_TTS_TIMEOUT);
						}
						s_tts_player->on_directive(s_tts_player, &audio, true);
						lisa_mem_free(url);
					} else {
						LISA_LOGE(TAG, "content_len < 10");
						if (s_nlp_result == NLP_RESULT_ERROR ||
								s_nlp_result == NLP_RESULT_UNSUPPORT ||
								s_nlp_result == NLP_RESULT_BROADCAST) {
							LISA_LOGE(TAG, "nlp error");
							char *url = app_tone_get_url(61);
							int url_len = strlen(url);
							memcpy(audio.m_url, url, url_len);
							audio.m_url[url_len] = '\0';
							s_tts_player->on_directive(s_tts_player, &audio, true);
						} else {
							audioplayer_t *audioplayer = s_audio_player;
							if (s_audio_list) {
								audioplayer->on_directive(
										audioplayer, AUDIO_PLAY, s_audio_list, s_audio_size);
								_release_audio_list();
							}
						}
					}
				}
			}

			if (text && cJSON_IsString(text) && strlen(text->valuestring) != 0) {
				assist_controller_trigger_event(CONTROLLER_EVENT_STATE_SESSION_END, NULL, 0);
			}
		} else {
			if (sub && cJSON_IsString(sub)) {
				if (strcmp(sub->valuestring, "nlp") == 0) {
					audio_out_t *audios = NULL;
					cJSON *intent = cJSON_GetObjectItem(data, "intent");
					const int result = _parser_audios(intent, &audios);
					if (result > 0) {
						s_nlp_result = NLP_RESULT_MEDIA;
						_release_audio_list();
						s_audio_list = audios;
						s_audio_size = result;
						assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PLAY_START, NULL, 0);
					} else {
						int ret = _parser_intent(intent);
						switch (ret) {
							case 0:
							case 1:
								s_nlp_result = NLP_RESULT_CONTROL;
								break;
							case 2:
								s_nlp_result = NLP_RESULT_BROADCAST;
								break;
							default:
								s_nlp_result = NLP_RESULT_UNSUPPORT;
								break;
						}
						if (ret == 0) {
							if (lisa_aiui_get_interactive_mode() == INTER_CONTINUE) {
								recognizer_recognize_once_end(s_rec);
							} else {
								recognizer_recognize_end(s_rec);
							}
							listen_audioplayer_resume(s_audio_player);
						}
					}
				} else if (strcmp(sub->valuestring, "iat") == 0) {
					cJSON *text = cJSON_GetObjectItem(data, "text");
					if (text && cJSON_IsString(text)) {
						LISA_LOGI(TAG, "iat text:%s", text->valuestring);
						assist_controller_trigger_event(CONTROLLER_EVENT_STATE_CLOUD_UPDATE_IAT_TEXT, text->valuestring, strlen(text->valuestring)+1);
					}

					cJSON *result_id = cJSON_GetObjectItem(data, "result_id");
					if (result_id && cJSON_IsNumber(result_id) == 1) {
						LISA_LOGI(TAG, "iat result_id:%d", result_id->valueint);
						// stop tts_player
						s_tts_player->stop(s_tts_player);
					}
				}
			}  // if (sub && cJSON_IsString(sub))
		}
	} else if (strcmp(action->valuestring, "finish") == 0) {
		cJSON *rid = cJSON_GetObjectItem(root, "rid");
		if (rid && cJSON_IsString(rid)) {
			char *rid_str = cJSON_GetObjectItem(root, "rid")->valuestring;
			uint32_t rid = (uint32_t)strtoul(rid_str, NULL, 10);
			uint32_t type = LISA_AIUI_FRAME_TYPE_UNKNOW;
			uint32_t *data;
			if (lisa_aiui_rid_list_get(rid, (void **)&data) == 0) {
				type = (uint32_t)data;
				lisa_aiui_rid_list_remove(rid);
			} else {
				LISA_LOGE(TAG, "rid: %u not found", rid);
			}

			LISA_LOGW(TAG, "session finished, rid: %u, type: %d", rid, type);

			if ((type == LISA_AIUI_FRAME_TYPE_AUDIO) || (type == LISA_AIUI_FRAME_TYPE_IMAGE)) {
				recognizer_stop_record(s_rec);
				recognizer_recognize_end(s_rec);
				listen_audioplayer_puse(s_audio_player);
				assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PRE_IDLE, NULL, 0);

				if (lisa_aiui_get_interactive_mode() == INTER_CONTINUE) {
					lisa_timer_stop(tts_timer);
				}
			}
		}

		// if (strcmp(curr_tts_cid, cJSON_GetObjectItem(root, "cid")->valuestring) == 0) {
		// 	LISA_LOGI(TAG, "tts finish, cid: %s", cJSON_GetObjectItem(root, "cid")->valuestring);
		// } else {
		// 	recognizer_stop_record(s_rec);
		// 	recognizer_recognize_end(s_rec);
		// 	if (lisa_aiui_get_interactive_mode() == INTER_CONTINUE) {
		// 		lisa_timer_stop(tts_timer);
		// 		// play_timeout_audio();
		// 	}
		// 	assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PRE_IDLE, NULL, 0);
		// 	LISA_LOGW(TAG, "Interactive has end!!!");
		// }
	} else if (strcmp(action->valuestring, "error") == 0) {
		// 1. stop interactive
		lisa_timer_stop(tts_timer);
		recognizer_stop_record(s_rec);
		recognizer_recognize_end(s_rec);
		assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PRE_IDLE, NULL, 0);
		LISA_LOGW(TAG, "Interactive has end!!!");
		// 2. deal 401
		cJSON *err_code = cJSON_GetObjectItem(root, "code");
		if (!err_code) {
			LISA_LOGE(TAG, "invalid error frame");
		} else {
			if (strcmp(err_code->valuestring, "401") == 0) {  // Token invalid
				LISA_LOGE(TAG, "invalid token, updating");
				app_cloud_token_error();
			}
		}
		app_cloud_disconnect();
		// 4. play tone
		play_net_error_audio();
	} else if (strcmp(action->valuestring, "vad") == 0) {
		recognizer_stop_record(s_rec);
		assist_controller_trigger_event(CONTROLLER_EVENT_STATE_VAD_END, NULL, 0);
	} else if (strcmp(action->valuestring, "connected") == 0) {
		app_cloud_connect();
	}
PARSER_END:
	cJSON_Delete(root);
	lisa_mem_free(arg);
	return 0;
}
