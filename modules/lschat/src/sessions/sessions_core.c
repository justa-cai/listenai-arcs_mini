#define TAG "sessions_core"

#include <stdio.h>
#include "lsc_errno.h"
#include "lsc_conn.h"
#include "lsc_common.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_mutex.h"
#include "lsc_sessions_core.h"
#include "lsc_base64.h"
#include "cc/slist.h"
#include "cJSON.h"
#include "lisa_time.h"
#include "lsc_session_voice.h"

typedef struct {
	SList *slist;
	int session_cnt;
	session_t *cur_session;
	lsc_conn_t *conn;
	lisa_mutex_t *lock;
} sessions_core_t;

static sessions_core_t *g_sessions_core_obj;

static int _register_session(session_t *session)
{
	sessions_core_t *core = g_sessions_core_obj;

	enum cc_stat stat = slist_add(core->slist, session);
	if (stat != CC_OK) {
		return LSC_ERR;
	}

	core->session_cnt += 1;

	return LSC_OK;
}

static int _remove_session(session_t *session)
{
	sessions_core_t *core = g_sessions_core_obj;

	enum cc_stat stat = slist_remove(core->slist, session, NULL);
	if (stat != CC_OK) {
		return LSC_ERR;
	}

	if (core->session_cnt > 0) {
		core->session_cnt -= 1;
	}

	return LSC_OK;
}

static session_t *_foreach_session_from_rid(uint32_t rid)
{
	sessions_core_t *core = g_sessions_core_obj;

	SLIST_FOREACH(s, core->slist, {
		if (((session_t *)s)->rid == rid) {
			return (session_t *)s;
		}
	});

	return NULL;
}

static inline int session_parse_tts(cJSON *data, session_t *ss)
{
	cJSON *content = cJSON_GetObjectItem(data, "content");
	CHECK_COND_GOTO(content, _err, "parse sub item faild");
	int content_len = strlen(content->valuestring);
	int decLen = content_len / 4.0 * 3 + 8;
	char *url = (char *)lisa_mem_calloc(1, sizeof(char) * decLen);
	int out_len = 0;
	int ret = lsc_base64_decode(content->valuestring, content_len, url, &out_len);
	if (ret != 0) {
		LISA_NLOGE("tts url decode failed, %s  %d", content->valuestring, ret);
		lisa_mem_free(url);
		return LSC_ERR;
	}

	// debug check
	if (out_len >= decLen) {
		LISA_NLOGE("maybe mem cross(%d > %d)", out_len, decLen);
	}

	LISA_NLOGI("--- tts url: %s", url);

	lisa_evt_publisher_publish(ss->pub, SESSION_TTS_URL, url, strlen(url) + 1);
	lisa_mem_free(url);

	return LSC_OK;
_err:
	return LSC_ERR;
}

static inline int session_parse_nlp_music(cJSON *data, session_t *ss)
{
	cJSON *intent = cJSON_GetObjectItem(data, "intent");

	cJSON *semantic = cJSON_GetObjectItem(intent, "semantic");
	CHECK_COND_GOTO(semantic, _err, "parse item faild");

	CHECK_COND_GOTO(cJSON_GetArraySize(semantic) > 0, _err, "array size 0");

	cJSON *item = cJSON_GetArrayItem(semantic, 0);
	CHECK_COND_GOTO(item, _err, "parse item faild");

	cJSON *control = cJSON_GetObjectItem(item, "intent");
	CHECK_COND_GOTO(control, _err, "parse item faild");

	if (!strcmp(control->valuestring, "RANDOM_SEARCH") || !strcmp(control->valuestring, "PLAY")) {
		cJSON *intent_data = cJSON_GetObjectItem(intent, "data");
		CHECK_COND_GOTO(intent_data, _err, "parse item faild");

		cJSON *intent_result = cJSON_GetObjectItem(intent_data, "result");
		CHECK_COND_GOTO(intent_result, _err, "parse item faild");

		int size = cJSON_GetArraySize(intent_result);
		CHECK_COND_GOTO(size > 0, _err, "no music lists");

		session_music_lists_t *lists =
			lisa_mem_calloc(1, sizeof(session_music_lists_t) + size * sizeof(session_music_item_t));
		CHECK_COND_GOTO(lists, _err, "no mem");

		lists->cnt = size;

		for (int i = 0; i < size; i++) {
			cJSON *item = cJSON_GetArrayItem(intent_result, i);

			cJSON *itemid = cJSON_GetObjectItem(item, "itemid");
			strcpy(lists->items[i].id, itemid->valuestring);

			// cJSON *uni_url = cJSON_GetObjectItem(item, "uni_url");
			// strcpy(lists->items[i].url, uni_url->valuestring);

			cJSON *playable = cJSON_GetObjectItem(item, "playable");
			lists->items[i].playable = (playable->valueint == 1) ? true : false;

			LISA_NLOGI("music_%d (id:%s)", i, lists->items[i].id);

			// cJSON *duration = cJSON_GetObjectItem(item, "duration");
			// cJSON *allRate = cJSON_GetObjectItem(item, "allRate");
			// cJSON *name = cJSON_GetObjectItem(item, "name");
		}

		lisa_evt_publisher_publish(ss->pub, SESSION_MUSIC_LISTS, lists,
					   sizeof(session_music_lists_t) + size * sizeof(session_music_item_t));

		if (lists) {
			lisa_mem_free(lists);
		}
	} else if (!strcmp(control->valuestring, "INSTRUCTION")) {
		cJSON *slots = cJSON_GetObjectItem(item, "slots");
		CHECK_COND_GOTO(slots, _err, "parse item faild");

		int size = cJSON_GetArraySize(slots);
		CHECK_COND_GOTO(size > 0, _err, "no music instruction");

		/* 获取命令类型 */
		cJSON *control_cmd = NULL;
		for (int i = 0; i < size; i++) {
			cJSON *slot = cJSON_GetArrayItem(slots, i);

			cJSON *name = cJSON_GetObjectItem(slot, "name");
			if (!strcmp(name->valuestring, "insType")) {
				cJSON *value = cJSON_GetObjectItem(slot, "value");
				CHECK_COND_GOTO(value, _err, "parse item faild");
				control_cmd = value;
				break;
			}
		}
		CHECK_COND_GOTO(control_cmd, _err, "no music cmd");

		session_music_instr_t music_instr = {0};
		int vol = 0;
		if (!strcmp(control_cmd->valuestring, "next")) {
			music_instr.instr = SESSION_MUSIC_INSTR_NEXT;
		} else if (!strcmp(control_cmd->valuestring, "past")) {
			music_instr.instr = SESSION_MUSIC_INSTR_PAST;
		} else if (!strcmp(control_cmd->valuestring, "replay")) {
			music_instr.instr = SESSION_MUSIC_INSTR_REPLAY;
		} else if (!strcmp(control_cmd->valuestring, "close")) {
			music_instr.instr = SESSION_MUSIC_INSTR_CLOSE;
		} else if (!strcmp(control_cmd->valuestring, "volume_select")) {
			int i = 0;
			for (; i < size; i++) {
				cJSON *slot = cJSON_GetArrayItem(slots, i);
				cJSON *name = cJSON_GetObjectItem(slot, "name");

				if (!strcmp(name->valuestring, "series")) {
					cJSON *vol_value = cJSON_GetObjectItem(slot, "value");
					vol = atoi(vol_value->valuestring);
					break;
				}
			}
			if (i < size) {
				music_instr.arg = vol;
				music_instr.instr = SESSION_MUSIC_INSTR_VOL_SET;
			}
		}

		lisa_evt_publisher_publish(ss->pub, SESSION_MUSIC_INSTR, &music_instr, sizeof(session_music_instr_t));
	}

	return LSC_OK;
_err:
	return LSC_ERR;
}

static inline int session_parse_nlp(cJSON *data, session_t *ss)
{
	char *nlp_data = cJSON_PrintUnformatted(data);
	if (nlp_data) {
		lisa_evt_publisher_publish(ss->pub, SESSION_NLP_RAW, nlp_data, strlen(nlp_data) + 1);
		cJSON_free(nlp_data);
	} else {
		LISA_NLOGE("%s:%d no mem", __FUNCTION__, __LINE__);
	}

	cJSON *nlp_origin = cJSON_GetObjectItem(data, "nlp_origin");
	CHECK_COND_GOTO(nlp_origin, _err, "parse item faild");

	cJSON *intent = cJSON_GetObjectItem(data, "intent");
	if (intent) {
		cJSON *service = cJSON_GetObjectItem(intent, "service");
		CHECK_COND_GOTO(service, _err, "parse item faild");

		if (!strcmp(service->valuestring, "bgColor")) {

		} else if (!strcmp(service->valuestring, "musicX")) {

			session_parse_nlp_music(data, ss);

		} else if (!strcmp(service->valuestring, "AIUI.control")) {
			char send_str[100];
			cJSON *semantic = cJSON_GetObjectItem(intent, "semantic");
			CHECK_COND_GOTO(semantic, _err, "parse item faild");

			CHECK_COND_GOTO(cJSON_GetArraySize(semantic) > 0, _err, "array size 0");

			cJSON *item = cJSON_GetArrayItem(semantic, 0);
			CHECK_COND_GOTO(item, _err, "parse item faild");

			cJSON *control = cJSON_GetObjectItem(item, "intent");
			CHECK_COND_GOTO(control, _err, "parse item faild");

			snprintf(send_str, sizeof(send_str), "%s", control->valuestring);
			if (strlen(control->valuestring) >= 100) {
				LISA_NLOGE("cmd too long");
				CHECK_COND_GOTO(false, _err, "parse item faild");
			}
			strcat(send_str, ":");

			cJSON *slots = cJSON_GetObjectItem(item, "slots");
			CHECK_COND_GOTO(slots, _err, "parse item faild");

			cJSON *slot = cJSON_GetArrayItem(slots, 0);
			if (slot != NULL) {
				{
					cJSON *value = cJSON_GetObjectItem(slot, "normValue");
					const char *value_str = cJSON_GetStringValue(value);
					if (strlen(value_str) > 100 - strlen(send_str) - 1) {
						LISA_NLOGE("value too long");
						CHECK_COND_GOTO(false, _err, "parse item faild");
					}
					strncat(send_str, value_str, strlen(value_str));
				}
			} else {
				if (strlen("NULL") > 100 - strlen(send_str) - 1) {
					LISA_NLOGE("cmd too long");
					CHECK_COND_GOTO(false, _err, "parse item faild");
				}
				strcat(send_str, "NULL");
			}

			lisa_evt_publisher_publish(ss->pub, SESSION_AIUI_CTR, send_str, strlen(send_str) + 1);
		} else if (!strcmp(service->valuestring, "scheduleX")) {
			char *intent_str = cJSON_PrintUnformatted(intent);
			if (intent_str) {
				LISA_NLOGI("Publishing alarm intent event");
				lisa_evt_publisher_publish(ss->pub, SESSION_ALARM_INTENT, intent_str,
							   strlen(intent_str) + 1);
				cJSON_free(intent_str);
			} else {
				LISA_NLOGE("Failed to serialize alarm intent JSON");
			}
		} else {
			// CHECK_COND_GOTO(false, _err, "parse item faild");
		}
	} else {
		if (!strcmp(nlp_origin->valuestring, "reply_text")) {
			cJSON *nlp = cJSON_GetObjectItem(data, "nlp");
			CHECK_COND_GOTO(nlp, _err, "parse nlp item faild");

			cJSON *stream_url = cJSON_GetObjectItem(nlp, "stream_url");
			CHECK_COND_GOTO(stream_url, _err, "parse stream_url item faild");

			LISA_NLOGI("--- reply text url: %s", stream_url->valuestring);
			lisa_evt_publisher_publish(ss->pub, SESSION_REPLY_URL, stream_url->valuestring,
						   strlen(stream_url->valuestring) + 1);
		} else if (!strcmp(nlp_origin->valuestring, "image_generation")) {
			cJSON *iner_data = cJSON_GetObjectItem(data, "data");
			CHECK_COND_GOTO(iner_data, _err, "parse item faild");

			cJSON *result = cJSON_GetObjectItem(iner_data, "result");
			CHECK_COND_GOTO(result, _err, "parse item faild");

			cJSON *item = cJSON_GetArrayItem(result, 0);
			CHECK_COND_GOTO(item, _err, "parse item faild");

			cJSON *draw_url = cJSON_GetObjectItem(item, "url");
			CHECK_COND_GOTO(draw_url, _err, "parse item faild");

			LISA_NLOGI("--- LLMDrawing url=%s", draw_url->valuestring);
			lisa_evt_publisher_publish(ss->pub, SESSION_DRAW_URL, draw_url->valuestring,
						   strlen(draw_url->valuestring) + 1);
		}
	}

	return LSC_OK;
_err:
	return LSC_ERR;
}


static inline int session_parse_vpr_info(cJSON *data, session_t *ss)
{
	cJSON *gender = cJSON_GetObjectItem(data, "gender");
	CHECK_COND_GOTO(gender, _err, "parse sub gender item faild");
	CHECK_COND_GOTO(gender->valuestring, _err, "parse sub gender value faild");

	char *data_str = cJSON_PrintUnformatted(data);
	lisa_evt_publisher_publish(ss->pub, SESSION_VPR_INFO, data_str, strlen(data_str) + 1);
	cJSON_free(data_str);

	return LSC_OK;
_err:
	return LSC_ERR;
}

static inline int session_parse_vpr_feature(cJSON *data, session_t *ss)
{
	cJSON *feature_id = cJSON_GetObjectItem(data, "feature_id");
	CHECK_COND_GOTO(feature_id, _err, "parse sub feature_id item faild");
	CHECK_COND_GOTO(feature_id->valuestring, _err, "parse sub feature_id value faild");

	char *data_str = cJSON_PrintUnformatted(data);
	lisa_evt_publisher_publish(ss->pub, SESSION_VPR_FEATURE, data_str, strlen(data_str) + 1);
	cJSON_free(data_str);

	return LSC_OK;
_err:
	return LSC_ERR;
}

static inline int session_parse_result(cJSON *root, session_t *ss)
{
	cJSON *data = cJSON_GetObjectItem(root, "data");
	CHECK_COND_GOTO(data, _err, "parse data item faild");

	cJSON *sub = cJSON_GetObjectItem(data, "sub");
	CHECK_COND_GOTO(sub, _err, "parse sub item faild");

	if (!strcmp(sub->valuestring, "vad")) {
		lisa_evt_publisher_publish(ss->pub, SESSION_GOT_VAD, NULL, 0);
	} else if (!strcmp(sub->valuestring, "iat")) {
		cJSON *text = cJSON_GetObjectItem(data, "text");
		CHECK_COND_GOTO(text, _err, "parse sub item faild");

		cJSON *result_id = cJSON_GetObjectItem(data, "result_id");
		if (result_id && cJSON_IsNumber(result_id) && result_id->valueint == 1 && text && cJSON_IsString(text) && strlen(text->valuestring) > 0) {
			lisa_evt_publisher_publish(ss->pub, SESSION_IAT_START, NULL, 0);
		}

		lisa_evt_publisher_publish(ss->pub, SESSION_IAT_TXT, text->valuestring, strlen(text->valuestring) + 1);

		cJSON *is_last = cJSON_GetObjectItem(data, "is_last");
		if (is_last && cJSON_IsTrue(is_last)) {
			lisa_evt_publisher_publish(ss->pub, SESSION_IAT_END, NULL, 0);
		}
	} else if (!strcmp(sub->valuestring, "tts")) {
		session_parse_tts(data, ss);
	} else if (!strcmp(sub->valuestring, "nlp")) {
		session_parse_nlp(data, ss);
	}
	else if(!strcmp(sub->valuestring, "vpr_info")) {
		session_parse_vpr_info(data, ss);
	}
	else if(!strcmp(sub->valuestring, "vpr_feature")){
		session_parse_vpr_feature(data, ss);
	}
	else {
		LISA_NLOGI("unparsed data(sub:%s)", sub->valuestring);
	}

	return LSC_OK;
_err:
	return LSC_ERR;
}

static void sessions_core_conn_evt_cb(conn_event_e evt, void *data, uint32_t size, void *usr)
{
	if (evt != CONN_DATA_CJSON){
		return;
	}

	sessions_core_t *core = g_sessions_core_obj;
	cJSON *root = (cJSON *)data;
	CHECK_COND_RETURN(root, "json parse faild");

	cJSON *root_rid = cJSON_GetObjectItem(root, "rid");
	if (root_rid == NULL) {
		LISA_NLOGW("no request id");
		return;
	}
	lisa_mutex_lock(core->lock, LISA_WAIT_FOREVER);
	uint32_t rid = (uint32_t)strtoul(root_rid->valuestring, NULL, 10);
	session_t *ss = _foreach_session_from_rid(rid);
	CHECK_COND_GOTO(ss, _err, "Unexpected rid(%u)", rid);

	char *raw_data = cJSON_PrintUnformatted(root);
	lisa_evt_publisher_publish(ss->pub, SESSION_RESULT_RAW_DATA, raw_data, strlen(raw_data) + 1);
	cJSON_free(raw_data);

	cJSON *root_action = cJSON_GetObjectItem(root, "action");
	CHECK_COND_RETURN(root_action, "no found action(rid:%d)", rid);
	if (!strcmp(root_action->valuestring, "started")) {
		lisa_evt_publisher_publish(ss->pub, SESSION_STARTED, NULL, 0);
	} else if (!strcmp(root_action->valuestring, "finish")) {
		lisa_evt_publisher_publish(ss->pub, SESSION_FINISH, NULL, 0);
		lisa_timer_stop(ss->timer);
	} else if (!strcmp(root_action->valuestring, "error")) {
		cJSON *err_code = cJSON_GetObjectItem(root, "code");
		if (!err_code) {
			LISA_NLOGE("invalid error frame");
		} else {
			LISA_NLOGE("error frame code:%s", err_code->valuestring);
			if (strcmp(err_code->valuestring, "401") == 0) {
				lisa_evt_publisher_publish(ss->pub, SESSION_TOKEN_INVALIDATION, NULL, 0);
			}
		}
		lisa_evt_publisher_publish(ss->pub, SESSION_ERR_FRAME, NULL, 0);
		lisa_timer_stop(ss->timer);
	} else if (!strcmp(root_action->valuestring, "result")) {
		session_parse_result(root, ss);
	} else if (!strcmp(root_action->valuestring, "ping")) {

	} else if (!strcmp(root_action->valuestring, "closed")) {

	} else {
	}

_err:
	lisa_mutex_unlock(core->lock);
}

static void session_destroy_start_frame(char *frame)
{
	if (frame) {
		cJSON_free(frame);
	}
}

static char *session_build_start_frame(uint32_t rid, session_params_t *cfg, uint8_t *img)
{
	cJSON *root = cJSON_CreateObject();

	cJSON_AddItemToObject(root, "action", cJSON_CreateString("start"));

	cJSON *params = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "params", params);

	if (cfg->full_duplex) {
		cJSON_AddItemToObject(params, "fullduplex", cJSON_CreateString("1"));
		if (cfg->full_duplex_timeout_s) {
			char timeout_str[12];
			snprintf(timeout_str, sizeof(timeout_str), "%u", cfg->full_duplex_timeout_s);
			cJSON_AddItemToObject(params, "fullduplex_timeout", cJSON_CreateString(timeout_str));
		}
	}

	char *data_type = cfg->data_type;
	cJSON_AddItemToObject(params, "data_type", cJSON_CreateString(data_type));

	if (strlen(cfg->aue)) {
		cJSON_AddItemToObject(params, "aue", cJSON_CreateString(cfg->aue));
	}

	if (cfg->speex_size > 0) {
		cJSON_AddNumberToObject(params, "speex_size", cfg->speex_size);
	}

	if (cfg->nlu_params.enable || cfg->tts_params.enable) {
		cJSON *features = cJSON_CreateArray();
		if (cfg->nlu_params.enable) {
			cJSON_AddItemToArray(features, cJSON_CreateString("nlu"));
		}
		if (cfg->tts_params.enable) {
			cJSON_AddItemToArray(features, cJSON_CreateString("tts"));
		}
		cJSON_AddItemToObject(params, "features", features);
	}

	cJSON *nlu_properties = cJSON_CreateObject();
	cJSON_AddItemToObject(params, "nlu_properties", nlu_properties);
	cJSON *custom = cJSON_CreateObject();
	if (cfg->nlu_properties.enable) {
		cJSON *abilities = cJSON_CreateArray();
		cJSON *intents = cJSON_CreateArray();
		cJSON *ability = cJSON_CreateObject();
		cJSON_AddItemToObject(nlu_properties, "abilities", abilities);
		cJSON_AddItemToArray(abilities, ability);
		cJSON_AddStringToObject(ability, "name", "alarm");
		cJSON_AddItemToObject(ability, "intents", intents);
		cJSON_AddItemToArray(intents, cJSON_CreateString("create"));
		cJSON_AddItemToArray(intents, cJSON_CreateString("cancel"));
		cJSON_AddStringToObject(nlu_properties, "lat", "");
		cJSON_AddStringToObject(nlu_properties, "lng", "");
		cJSON_AddStringToObject(custom, "userID", "");
	}
	cJSON_AddItemToObject(nlu_properties, "custom", custom);

	/* 添加当前选择的模型ID到请求中 */
	// char current_model_id[64] = {0};
	// /* 使用session_voice_get_model_id函数获取当前模型ID */
	// if (session_voice_get_model_id(current_model_id, sizeof(current_model_id)) == 0) {
	// 	if (strlen(current_model_id) > 0) {
	// 		LISA_NLOGI("Adding model ID to request: %s", current_model_id);
	// 		/* 创建llmParams对象 */
	// 		cJSON *llmParams = cJSON_CreateObject();
	// 		cJSON_AddItemToObject(custom, "llmParams", llmParams);
	// 		/* 将模型ID添加到llmParams.model字段 */
	// 		cJSON_AddItemToObject(llmParams, "model", cJSON_CreateString(current_model_id));
	// 	}
	// } else {
	// 	LISA_NLOGD("Failed to get model ID or no model ID set");
	// }
	if (img) {
		cJSON_AddItemToObject(custom, "image", cJSON_CreateString(img));
	}

	char rid_buf[12]; // 足够存放10位数字和终止符'\0'
	snprintf(rid_buf, sizeof(rid_buf), "%u", rid);
	cJSON_AddItemToObject(custom, "rid", cJSON_CreateString(rid_buf));

	if (strlen(cfg->nlu_params.sn)) {
		cJSON_AddItemToObject(nlu_properties, "sn", cJSON_CreateString(cfg->nlu_params.sn));
	}

	if (cfg->tts_params.enable) {
		// todo
	}

	if (cfg->nlu_params.enable) {
		// todo
	}

	if (cfg->asr_params.enable) {
		cJSON *asr_properties = cJSON_CreateObject();
		cJSON_AddItemToObject(params, "asr_properties", asr_properties);
		if (!cfg->asr_params.vad_enable) {
			cJSON_AddItemToObject(asr_properties, "evad", cJSON_CreateString("0"));
			cJSON_AddItemToObject(asr_properties, "svad", cJSON_CreateString("0"));
		}

		cJSON_AddStringToObject(asr_properties, "audio_gain", "6.0");

		if (cfg->asr_params.oneshot) {
			cJSON_AddItemToObject(asr_properties, "oneshot", cJSON_CreateString("1"));
		}

		if (cfg->asr_params.oneshot && cfg->asr_params.words_cnt > 0) {
			cJSON *wake_properties = cJSON_CreateObject();
			cJSON_AddItemToObject(params, "wake_properties", wake_properties);

			cJSON *words = cJSON_CreateArray();
			for (int i = 0; i < cfg->asr_params.words_cnt; i++) {
				cJSON_AddItemToArray(words, cJSON_CreateString(cfg->asr_params.words[i]));
			}
			cJSON_AddItemToObject(wake_properties, "words", words);
		}
	}

	char *out = cJSON_PrintUnformatted(root);
	LISA_NLOGI("start frame:%s", out);

	cJSON_Delete(root);

	return out;
}

void _print_session_config(session_params_t *cfg)
{
	LISA_NLOGD("full_duplex=%d", cfg->full_duplex);
	LISA_NLOGD("data_type=%s", cfg->data_type);
	LISA_NLOGD("aue=%s", cfg->aue);
	LISA_NLOGD("nlu_params.enable=%d", cfg->nlu_params.enable);
	LISA_NLOGD("nlu_params.sn=%s", cfg->nlu_params.sn);
	LISA_NLOGD("tts_params.enable=%d", cfg->tts_params.enable);
	LISA_NLOGD("asr_params.enable=%d", cfg->asr_params.enable);
	LISA_NLOGD("asr_params.vad_enable=%d", cfg->asr_params.vad_enable);

	if (cfg->asr_params.words_cnt) {
		LISA_NLOGI("asr_params.words_cnt=%d", cfg->asr_params.words_cnt);
		for (int i = 0; i < cfg->asr_params.words_cnt; i++) {
			LISA_NLOGI("asr_params.words[%d]=%s", i, cfg->asr_params.words[i]);
		}
	}
}

int session_set_config(session_t *hdl, session_params_t *cfg)
{
	if (hdl->params.asr_params.words) {
		for (int i = 0; i < hdl->params.asr_params.words_cnt; i++) {
			if (hdl->params.asr_params.words[i]) {
				lisa_mem_free(hdl->params.asr_params.words[i]);
				hdl->params.asr_params.words[i] = NULL;
			}
		}
		lisa_mem_free(hdl->params.asr_params.words);
		hdl->params.asr_params.words = NULL;
	}

	memcpy((void *)&(hdl->params), (void *)cfg, sizeof(session_params_t));

	if (cfg->asr_params.words_cnt > 0) {
		hdl->params.asr_params.words = (char **)lisa_mem_calloc(cfg->asr_params.words_cnt, sizeof(char *));
		assert(hdl->params.asr_params.words);
		for (int i = 0; i < cfg->asr_params.words_cnt; i++) {
			hdl->params.asr_params.words[i] =
				lisa_mem_calloc(strlen(cfg->asr_params.words[i]) + 1, sizeof(char));
			assert(hdl->params.asr_params.words[i]);
			memcpy(hdl->params.asr_params.words[i], cfg->asr_params.words[i],
			       strlen(cfg->asr_params.words[i]) + 1);
		}
	}

	_print_session_config(&(hdl->params));

	return LSC_OK;
}

int session_get_config(session_t *hdl, session_params_t *cfg)
{
	memcpy((void *)cfg, (void *)&(hdl->params), sizeof(session_params_t));
	return LSC_OK;
}

int session_start(session_t *hdl, char *data)
{
	sessions_core_t *core = g_sessions_core_obj;
	uint32_t rid = lisa_rand32();

	char *text = session_build_start_frame(rid, &hdl->params, data);
	if (text == NULL) {
		LISA_NLOGE("build frame faild");
		return LSC_ERR;
	}

	int ret = core->conn->send_text(text);
	if (ret) {
		LISA_NLOGE("send start frame faild(ret=%d)", ret);
		session_destroy_start_frame(text);
		return LSC_ERR;
	}

	session_destroy_start_frame(text);

	hdl->rid = rid;

	lisa_timer_stop(hdl->timer);
	if (hdl->params.session_timeout != 0 && hdl->params.full_duplex == false) {
		lisa_timer_change_period(hdl->timer, hdl->params.session_timeout);
		hdl->timer->type = OS_TIMER_ONCE;
		lisa_timer_start(hdl->timer);
	}

	return LSC_OK;
}

int session_cancel(session_t *hdl)
{
#define CANCEL_FORMAT "{\"action\":\"cancel\"}"
	sessions_core_t *core = g_sessions_core_obj;

	int ret = core->conn->send_text(CANCEL_FORMAT);
	if (ret) {
		LISA_NLOGE("send cancle frame faild(ret=%d)", ret);
		return LSC_ERR;
	}

	return LSC_OK;
}

int session_send_bin(session_t *hdl, const uint8_t *data, uint32_t size)
{
	sessions_core_t *core = g_sessions_core_obj;

	int ret = core->conn->send_bin(data, size);
	if (ret) {
		LISA_NLOGE("send bin faild(ret=%d)", ret);
		return LSC_ERR;
	}

	return LSC_OK;
}

int session_end(session_t *hdl)
{
#define END_FORMAT "{\"action\":\"end\"}"
	sessions_core_t *core = g_sessions_core_obj;

	int ret = core->conn->send_text(END_FORMAT);
	if (ret) {
		LISA_NLOGE("send end frame faild(ret=%d)", ret);
		return LSC_ERR;
	}

	return LSC_OK;
}

static void session_timeout_cb(struct lisa_timer *timer)
{
	session_t *session = (session_t *)timer->arg;
	LISA_NLOGD("ss timeout !");
	if (session) {
		lisa_evt_publisher_publish(session->pub, SESSION_TIMEOUT, NULL, 0);
		// invalid rid value
		session->rid = 0;
	}
}

session_t *sessions_core_find_by_sid(const char *sid)
{
	sessions_core_t *core = g_sessions_core_obj;

	if (core == NULL || sid == NULL || sid[0] == '\0') {
		return NULL;
	}

	session_t *found = NULL;
	lisa_mutex_lock(core->lock, LISA_WAIT_FOREVER);

	SLIST_FOREACH(s, core->slist, {
		session_t *cur = (session_t *)s;
		/* sid 为会话建立后在数据流中下发并填充，非空再比较 */
		if (cur && cur->sid[0] != '\0' && strcmp(cur->sid, sid) == 0) {
			found = cur;
			break;
		}
	});

	lisa_mutex_unlock(core->lock);
	return found;
}

session_t *session_create(void)
{
	CHECK_COND_RETURN_VAL(g_sessions_core_obj, NULL, "not create");
	sessions_core_t *core = g_sessions_core_obj;

	lisa_mutex_lock(core->lock, LISA_WAIT_FOREVER);

	session_t *session = lisa_mem_calloc(1, sizeof(session_t));
	CHECK_COND_GOTO(session, _err, "no mem");

	_register_session(session);

	session->pub = lisa_evt_publisher_new();
	CHECK_COND_GOTO(session->pub, _err, "lisa new publisher faild");

	session->timer = lisa_timer_create(1000, session_timeout_cb, (void *)session);
	CHECK_COND_GOTO(session->timer, _err, "timer create faild");

	lisa_mutex_unlock(core->lock);

	return session;
_err:
	if (session) {
		lisa_mem_free(session);
	}

	_remove_session(session);

	if (session->pub) {
		lisa_evt_publisher_destroy(session->pub);
	}

	if (session->timer != NULL) {
		lisa_timer_delete(session->timer);
	}

	lisa_mutex_unlock(core->lock);

	return NULL;
}

int session_add_evt_callback(session_t *hdl, sessions_event_cb_t cb, sessions_event_e evt, void *usr)
{
	int ret = lisa_evt_publisher_evt_add(hdl->pub, evt, (lisa_evt_publisher_cb_t)cb, usr);
	CHECK_COND_RETURN_VAL(ret == 0, LSC_ERR, "evt add faild");

	return LSC_OK;
}

int session_destroy(session_t *hdl)
{
	session_t *session = hdl;
	sessions_core_t *core = g_sessions_core_obj;

	if (session->timer != NULL) {
		lisa_timer_delete(session->timer);
	}

	lisa_mutex_lock(core->lock, LISA_WAIT_FOREVER);

	if (session->pub) {
		lisa_evt_publisher_destroy(session->pub);
		session->pub = NULL;
	}

	_remove_session(hdl);

	lisa_mem_free(session);

	lisa_mutex_unlock(core->lock);

	return LSC_OK;
}

int sessions_core_init(lsc_conn_t *conn)
{
	CHECK_COND_RETURN_VAL(conn, LSC_INVALID_PARAM, "is null");

	sessions_core_t *core = lisa_mem_calloc(1, sizeof(sessions_core_t));
	CHECK_COND_GOTO(core, _err, "no mem");

	SListConf conf = {
		.mem_alloc = (void *(*)(size_t))lisa_mem_alloc,
		.mem_free = lisa_mem_free,
		.mem_calloc = (void *(*)(size_t, size_t))lisa_mem_calloc,
	};

	enum cc_stat stat = slist_new_conf(&conf, &core->slist);
	CHECK_COND_GOTO(stat == CC_OK, _err, "no mem");

	core->conn = conn;

	core->lock = lisa_mutex_create();

	core->conn->add_evt_callback(sessions_core_conn_evt_cb, CONN_DATA_CJSON, NULL);

	g_sessions_core_obj = core;

	return LSC_OK;
_err:
	if (core) {
		lisa_mem_free(core);
		if (core->slist) {
			slist_destroy(core->slist);
		}
	}

	return LSC_ERR;
}

int sessions_core_deinit(void)
{
	if (g_sessions_core_obj) {
		if (g_sessions_core_obj->slist) {
			slist_destroy(g_sessions_core_obj->slist);
		}

		lisa_mutex_delete(g_sessions_core_obj->lock);

		lisa_mem_free(g_sessions_core_obj);
		g_sessions_core_obj = NULL;
	}

	return LSC_OK;
}