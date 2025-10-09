#include <string.h>
#include <stdio.h>
#include "lisa_evs.h"
#include "lisa_evs_uuid.h"
#include "sockets/lisa_evs_websocket.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_http.h"
#include "lisa_typedef.h"
#include "utils/evs_utils.h"

#define TAG "lisa_evs_sync"

#define DICTIONARY_LOOKUP_STACK_SIZE (4 * 1024)
#define URL_DICTIONARY_LOOKUP ("https://api.iflyos.cn/external/ocr/dict?q=")
#define URL_EVALUATE_LEVEL ("https://api.iflyos.cn/external/ocr/evaluate/levels")
#define URL_EVALUATE_QUEST ("https://api.iflyos.cn/external/ocr/evaluate/get_question?")
#define URL_GET_OL_TTS_SPEED ("https://api.iflyos.cn/external/ocr/device/voice_config")
#define URL_SET_OL_TTS_SPEED ("https://api.iflyos.cn/external/ocr/device/update_voice_config")

#define HTTP_TIME_OUT (10)

typedef struct lisa_dictionary_lookup_context {
	const lisa_evs_t *m_handle;
	char* m_token;
	char* m_word;
	const lisa_dictionary_lookup_cb_t* m_cb;
} lisa_dictionary_lookup_context_t;

static lisa_dictionary_lookup_context_t *g_dictionary_lookup_context = NULL;

static lisa_http_t *g_dictionary_lookup_http = NULL;
static lisa_thread_t *g_dictionary_lookup_thread = NULL;
static void* s_access_token = NULL;

lisa_err_t lisa_evs_progress_sync(const lisa_evs_t *const handle, const lisa_evs_play_info_t *const progress)
{
	int ret = 0;
	if (progress->state == LISA_EVS_PLAYSTATE_PLAYING || progress->state == LISA_EVS_PLAYSTATE_STOPED ||
			progress->state == LISA_EVS_PLAYSTATE_FINISHED || progress->state == LISA_EVS_PLAYSTATE_ERROR) {
		char request_id[40];
		lisa_evs_uuid_generate_string(request_id);
		cJSON *proc_sync = cJSON_CreateObject();
		cJSON *header = cJSON_CreateObject();
		cJSON *payload = cJSON_CreateObject();
		cJSON_AddStringToObject(header, "name", "audio_player.playback.progress_sync");
		cJSON_AddStringToObject(header, "request_id", request_id);
		long offset = 0L;
		if (progress->state == LISA_EVS_PLAYSTATE_PLAYING) {
			cJSON_AddStringToObject(payload, "type", "STARTED");
			cJSON_AddStringToObject(payload, "resource_id", progress->resource_id);
			cJSON_AddItemToObject(proc_sync, "header", header);
			cJSON_AddItemToObject(proc_sync, "payload", payload);
			ret = lisa_evs_client_request(handle, proc_sync, request_id);
		} else if (progress->state == LISA_EVS_PLAYSTATE_STOPED) {
			offset = progress->offset;
			cJSON_AddStringToObject(payload, "resource_id", progress->resource_id);
			cJSON_AddStringToObject(payload, "type", "PAUSED");
			cJSON_AddNumberToObject(payload, "offset", offset);
			cJSON_AddItemToObject(proc_sync, "header", header);
			cJSON_AddItemToObject(proc_sync, "payload", payload);
			ret = lisa_evs_client_request(handle, proc_sync, request_id);
		} else if (progress->state == LISA_EVS_PLAYSTATE_FINISHED) {
			cJSON_AddStringToObject(payload, "resource_id", progress->resource_id);
			cJSON_AddStringToObject(payload, "type", "NEARLY_FINISHED");
			cJSON_AddNumberToObject(payload, "offset", offset);
			cJSON_AddItemToObject(proc_sync, "header", header);
			cJSON_AddItemToObject(proc_sync, "payload", payload);
			lisa_evs_client_request(handle, proc_sync, request_id);

			// send finished
			proc_sync = cJSON_CreateObject();
			header = cJSON_CreateObject();
			payload = cJSON_CreateObject();
			cJSON_AddStringToObject(header, "name", "audio_player.playback.progress_sync");
			cJSON_AddStringToObject(header, "request_id", request_id);
			cJSON_AddStringToObject(payload, "resource_id", progress->resource_id);
			cJSON_AddStringToObject(payload, "type", "FINISHED");
			cJSON_AddNumberToObject(payload, "offset", offset);
			cJSON_AddItemToObject(proc_sync, "header", header);
			cJSON_AddItemToObject(proc_sync, "payload", payload);
			ret = lisa_evs_client_request(handle, proc_sync, request_id);
		} else if (progress->state == LISA_EVS_PLAYSTATE_ERROR) {
			cJSON_AddStringToObject(payload, "type", "FAILED");
			cJSON_AddNumberToObject(payload, "failure_code", 1002);
			cJSON_AddStringToObject(payload, "resource_id", progress->resource_id);
			cJSON_AddItemToObject(proc_sync, "header", header);
			cJSON_AddItemToObject(proc_sync, "payload", payload);
			ret = lisa_evs_client_request(handle, proc_sync, request_id);
		}
	}
	return (ret == 0)? LISA_OK:LISA_FAIL;
}

lisa_err_t lisa_evs_system_sync(const lisa_evs_t *const handle)
{
	char request_id[40] = {0};
	lisa_evs_uuid_generate_string(request_id);

	cJSON *root = cJSON_CreateObject();
	cJSON *header = cJSON_CreateObject();
	cJSON *payload = cJSON_CreateObject();
	cJSON_AddStringToObject(header, "name", "system.state_sync");
	cJSON_AddStringToObject(header, "request_id", request_id);
	cJSON_AddItemToObject(root, "header", header);
	cJSON_AddItemToObject(root, "payload", payload);

	int ret = lisa_evs_client_request(handle, root, request_id);
	return (ret == 0)? LISA_OK:LISA_FAIL;
}

lisa_err_t lisa_evs_connect(const lisa_evs_t *const handle, const uint8_t *const token)
{
	int ret = lisa_evs_websocket_connect(handle->ws, handle->config->device_id, token);
	return (ret == 0)? LISA_OK:LISA_FAIL;
}

lisa_err_t lisa_evs_disconnect(const lisa_evs_t *const handle)
{
	int ret = lisa_evs_websocket_disconnect(handle->ws);
	return (ret == 0)? LISA_OK:LISA_FAIL;
}

lisa_err_t lisa_evs_upload_check_result(const lisa_evs_t *const handle, const lisa_evs_update_msg_t *const msg)
{
	if(!handle || !msg) return LISA_FAIL;

	LISA_LOGD(TAG, "lisa evs upload check result [BEGIN]");
	char request_id[40] = {0};
	lisa_evs_uuid_generate_string(request_id);

	cJSON *root = cJSON_CreateObject();
	cJSON *header = cJSON_CreateObject();
	cJSON *payload = cJSON_CreateObject();
	// Header
	cJSON_AddStringToObject(header, "request_id", request_id);
	if (msg->result == CHECK_SUCCEED) {
		if (msg->has_new_ver) {
			// 有新版本，根据当前是否在升级上报信息
			if (msg->ota_state == LISA_OTA_STATE_ONGOING) {
				cJSON_AddStringToObject(header, "name", "system.software_update_state_sync");
				cJSON_AddStringToObject(payload, "state", "STARTED");
			} else {
				cJSON_AddStringToObject(header, "name", "system.check_software_update_result");
				cJSON_AddStringToObject(payload, "result", "SUCCEED");
				cJSON_AddTrueToObject(payload, "need_update");
			}
			cJSON_AddStringToObject(payload, "version_name", msg->version_name);
			cJSON_AddStringToObject(payload, "update_description", msg->desc);
		} else {
			// 无新版本，不需要升级
			cJSON_AddStringToObject(header, "name", "system.check_software_update_result");
			cJSON_AddStringToObject(payload, "result", "SUCCEED");
			cJSON_AddFalseToObject(payload, "need_update");
		}
	} else {
		// 检查版本失败
		cJSON_AddStringToObject(header, "name", "system.check_software_update_result");
		cJSON_AddStringToObject(payload, "result", "FAILED");
	}

	cJSON_AddItemToObject(root, "header", header);
	cJSON_AddItemToObject(root, "payload", payload);

	int ret = lisa_evs_client_request(handle, root, request_id);
	LISA_LOGD(TAG, "lisa evs upload check result [END]");
	return (ret == 0)? LISA_OK:LISA_FAIL;
}

int lisa_evs_client_request(const lisa_evs_t *const handle, cJSON *request_json, const uint8_t *const req_id)
{
    int ret = 0;

    lisa_evs_state_t total_state = handle->get_state_cb->get_state();

	cJSON *root = cJSON_CreateObject();
	cJSON *header = cJSON_CreateObject();
	cJSON *context = cJSON_CreateObject();

	// Header
	char auth[88] = {0};
	sprintf(auth, "Bearer %s", handle->access_token);
	cJSON_AddStringToObject(header, "authorization", auth); // ACCESS Token可获取
	// Header - Device
	cJSON *device = cJSON_CreateObject();
	// Header - Device - ID
	cJSON_AddStringToObject(device, "device_id", handle->config->device_id);
	// Header - Device - Platform
	cJSON *devicePlatform = cJSON_CreateObject();
	cJSON_AddStringToObject(devicePlatform, "name", total_state.platform_info.platform_name); // Example: FreeRTOS
	cJSON_AddStringToObject(devicePlatform, "version", total_state.platform_info.platform_version); // Example: 10.2.1
	cJSON_AddItemToObject(device, "platform", devicePlatform);
	cJSON_AddItemToObject(header, "device", device);

	// Context
    cJSON *sys = cJSON_CreateObject();
	cJSON_AddStringToObject(sys, "version", total_state.system_state.version);
	cJSON_AddStringToObject(sys, "firmware_version", total_state.system_state.firmware_version);
	cJSON_AddItemToObject(context, "system", sys);
	// Context - REC
    cJSON *rec = cJSON_CreateObject();
	cJSON_AddStringToObject(rec, "version", total_state.recognizer_state.version); // Example: 1.1
	cJSON_AddItemToObject(context, "recognizer", rec);
	// Context - Audio Player
    cJSON *audio_player = cJSON_CreateObject();
	cJSON_AddStringToObject(audio_player, "version", total_state.playback_state.version); // Example: 1.2
	cJSON *playback = cJSON_CreateObject();
	char *state = "IDLE";
	switch(total_state.playback_state.state) {
		case LISA_EVS_PLAYSTATE_IDLE:
		case LISA_EVS_PLAYSTATE_ERROR: break;
		case LISA_EVS_PLAYSTATE_PAUSED:
		case LISA_EVS_PLAYSTATE_PLAYING: {
			cJSON_AddStringToObject(playback, "resource_id", total_state.playback_state.resource_id);
			cJSON_AddNumberToObject(playback, "offset", total_state.playback_state.offset);
			state = "PLAYING";
		} break;
		case LISA_EVS_PLAYSTATE_STOPED:
		case LISA_EVS_PLAYSTATE_FINISHED: {
			state = "PAUSED";
			cJSON_AddStringToObject(playback, "resource_id", total_state.playback_state.resource_id);
			cJSON_AddNumberToObject(playback, "offset", total_state.playback_state.offset);
		} break;
	}
	cJSON_AddStringToObject(playback, "state", state);
	cJSON_AddItemToObject(audio_player, "playback", playback);
	cJSON_AddItemToObject(context, "audio_player", audio_player);
    // Context - Speaker
    cJSON *speaker = cJSON_CreateObject();
	cJSON_AddStringToObject(speaker, "version", total_state.speaker_state.version); // Example: 1.0
	cJSON_AddNumberToObject(speaker, "volume", total_state.speaker_state.volume);
	cJSON_AddItemToObject(context, "speaker", speaker);

	cJSON_AddItemToObject(root, "iflyos_header", header);
	cJSON_AddItemToObject(root, "iflyos_context", context);
	cJSON_AddItemToObject(root, "iflyos_request", request_json);

	char *msg = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	if (msg == NULL) {
		LISA_LOGE(TAG, "send msg is null");
		return -1;
	}
	// LISA_LOGD(TAG, "sendtext-------->%s", msg);
    ret = lisa_evs_websocket_send_text(handle->ws, msg, strlen(msg), req_id);
	cJSON_free(msg);
	return ret;
}

static void dictionary_lookup_result_cb(lisa_http_data_t *data) {
	lisa_dictionary_lookup_context_t *ins = (lisa_dictionary_lookup_context_t*)data->user;

	if (ins && ins->m_cb) {
		ins->m_cb->on_dictionary_lookup_succ(data->buf);
	}
}

static void* get_dictionary_lookup_headers(void) {
	static char header[256];
	sprintf(header, "Authorization:Bearer %s", g_dictionary_lookup_context->m_token);
	return header;
}

static void _dictionary_lookup_thread(void* param)
{
	lisa_dictionary_lookup_context_t *ins = (lisa_dictionary_lookup_context_t*)param;
	lisa_http_request_t *http_param = NULL;
	char* url = NULL;
	if (ins->m_handle == NULL || (ins->m_token == NULL || ins->m_token[0] == '\0')) {
		if (ins->m_cb) {
			ins->m_cb->on_dictionary_lookup_failed();
		}
		goto ERR;
	}

	if (g_dictionary_lookup_http) {
		LISA_LOGE(TAG, "error:  http has exist");
		if (ins->m_cb) {
			ins->m_cb->on_dictionary_lookup_failed();
		}
		goto ERR;
	}
	http_param = (lisa_http_request_t *)lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	if (http_param == NULL) {
		LISA_LOGE(TAG, "malloc http params error");
		if (ins->m_cb) {
			ins->m_cb->on_dictionary_lookup_failed();
		}
		goto ERR;
	}
	char out_str[128] = {0};
	lisa_evs_urlencode(ins->m_word, strlen(ins->m_word), out_str, sizeof(out_str));
	url = (char*)lisa_mem_calloc(1, sizeof(char) * (strlen(out_str) + strlen(URL_DICTIONARY_LOOKUP) + 1));
	if (!url) {
		LISA_LOGE(TAG, "malloc http params error");
		if (ins->m_cb) {
			ins->m_cb->on_dictionary_lookup_failed();
		}
		goto ERR;
	}
	sprintf(url, "%s%s", URL_DICTIONARY_LOOKUP, out_str);

	http_param->method = LISA_HTTP_GET;
	http_param->url = url;
	http_param->headers = (void*)get_dictionary_lookup_headers;
	http_param->timeout = HTTP_TIME_OUT;
	http_param->body_len = 0;
	http_param->body = NULL;
	http_param->on_data = (void *)dictionary_lookup_result_cb;

	http_param->user = (void *)ins;
	g_dictionary_lookup_http = lisa_http_init(http_param);
	if (!g_dictionary_lookup_http) {
		LISA_LOGE(TAG, " http  error");
		if (ins->m_cb) {
			ins->m_cb->on_dictionary_lookup_failed();
		}
		goto ERR;
	}

	if (lisa_http_perform(g_dictionary_lookup_http) != 0) {
		LISA_LOGE(TAG, "http get request info err..");
		if (ins->m_cb) {
			ins->m_cb->on_dictionary_lookup_failed();
		}
		goto ERR;
	}

ERR:
	if (url) {
		lisa_mem_free(url);
	}
	if (g_dictionary_lookup_http) {
		lisa_http_cleanup(g_dictionary_lookup_http);
		g_dictionary_lookup_http = NULL;
	}
	if (http_param) {
		lisa_mem_free(http_param);
	}
	if (ins) {
		if (ins->m_token) {lisa_mem_free(ins->m_token);}
		if (ins->m_word) {lisa_mem_free(ins->m_word);}
		lisa_mem_free(ins);
		g_dictionary_lookup_context = NULL;
	}
	if (g_dictionary_lookup_thread) {
		lisa_thread_delete(g_dictionary_lookup_thread);
		g_dictionary_lookup_thread = NULL;
	}
    return;
}

lisa_err_t lisa_dictionary_lookup(const lisa_evs_t *const handle, const uint8_t *const word, const lisa_dictionary_lookup_cb_t *const cb)
{
	if (!handle || !word || !handle->access_token || !cb) return LISA_FAIL;

	if (g_dictionary_lookup_thread) {
		LISA_LOGE(TAG, "lisa dictionary lookup request running");
		return LISA_FAIL;
	}
	char* request_word = (char*)lisa_mem_calloc(1, sizeof(char) * (strlen(word) + 1));
	if (!request_word) {
		return LISA_FAIL;
	}
	strcpy(request_word, word);
	lisa_dictionary_lookup_context_t *request_context = (lisa_dictionary_lookup_context_t*)lisa_mem_calloc(1, sizeof(lisa_dictionary_lookup_context_t));
	request_context->m_handle = handle;
	request_context->m_word = request_word;
	request_context->m_cb = cb;
	char* access_token = (char*)lisa_mem_calloc(1, sizeof(char) * (strlen(handle->access_token) + 1));
	strcpy(access_token, handle->access_token);
	request_context->m_token = access_token;
	g_dictionary_lookup_context = request_context;

	lisa_thread_attr_t thread_attr;
	thread_attr.name = "lisa_dictionary_lookup_thread";
	thread_attr.stack_size = DICTIONARY_LOOKUP_STACK_SIZE;
	thread_attr.priority = LISA_OS_PRIORITY_NORMAL; // OS_PRIORITY_NORMAL
	g_dictionary_lookup_thread = lisa_thread_create(&thread_attr, _dictionary_lookup_thread, (void *)request_context);
	if (g_dictionary_lookup_thread == NULL) {
		if (request_context) {
			if (request_context->m_word) {lisa_mem_free(request_context->m_word);}
			if (request_context->m_token) {lisa_mem_free(request_context->m_token);}
			lisa_mem_free(request_context);
		}
		LISA_LOGE(TAG, "lisa auth thread_create failed");
		return -1;
	}

	return LISA_OK;
}


static void* get_default_headers(void) {
	static char header[512];
	sprintf(header, "Authorization:Bearer %s\r\nContent-Type:application/json;charset=utf-8", s_access_token);
	return header;
}

static void get_evaluate_data_cb(lisa_http_data_t *data) {
	lisa_http_resp_cb cb = (lisa_http_resp_cb)data->user;
	cb(data->buf, data->len);
}

lisa_err_t lisa_get_evaluate_level(const lisa_evs_t *const handle, const lisa_http_resp_cb cb)
{
	if (!handle || !handle->access_token || !cb) return LISA_FAIL;

	lisa_http_request_t *http_param = NULL;
	http_param = lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	if (!http_param) return LISA_FAIL;

	http_param->method = LISA_HTTP_GET;
	unsigned char* url = (unsigned char*)lisa_mem_alloc(strlen(URL_EVALUATE_LEVEL) + 1);
	if (!url) {
		lisa_mem_free(http_param);
		return LISA_FAIL;
	}
	strcpy(url, URL_EVALUATE_LEVEL);
	http_param->url = url;
	http_param->headers = (void *)get_default_headers;
	http_param->timeout = HTTP_TIME_OUT;
	http_param->body_len = 0;
	http_param->body = NULL;
	s_access_token = handle->access_token;
	http_param->user = cb;
	http_param->on_data = (void *)get_evaluate_data_cb;

	lisa_http_t* http_req = lisa_http_init(http_param);
	if (!http_req) {
		LISA_LOGE(TAG, "http init error");
		lisa_mem_free(url);
		lisa_mem_free(http_param);
		return LISA_FAIL;
	}
	
	lisa_err_t ret = LISA_OK;
	if (lisa_http_perform(http_req) != LISA_HTTP_OK) {
		LISA_LOGE(TAG, "http get request info err..");
		ret = LISA_FAIL;
	}

	lisa_http_cleanup(http_req);
	lisa_mem_free(url);
	lisa_mem_free(http_param);
	return ret;
}

lisa_err_t lisa_get_evaluate_quest(const lisa_evs_t *const handle, const uint8_t *const lev, const uint8_t *const type, const lisa_http_resp_cb cb)
{
	if (!handle || !handle->access_token || !lev || !type || !cb) 
		return LISA_FAIL;
	
	lisa_http_request_t *http_param = NULL;
	http_param = lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	if (!http_param) return LISA_FAIL;

	http_param->method = LISA_HTTP_GET;
	unsigned char* url = (unsigned char*)lisa_mem_alloc(strlen(URL_EVALUATE_QUEST) + strlen("level_id=&type=") + strlen(lev) + strlen(type) + 1);
	if (!url) {
		lisa_mem_free(url);
		return LISA_FAIL;
	}
	sprintf(url, "%slevel_id=%s&type=%s", URL_EVALUATE_QUEST, lev, type);
	http_param->url = url;
	http_param->headers = (void *)get_default_headers;
	http_param->timeout = HTTP_TIME_OUT;
	http_param->body_len = 0;
	http_param->body = NULL;
	s_access_token = handle->access_token;
	http_param->user = cb;
	http_param->on_data = (void *)get_evaluate_data_cb;

	lisa_http_t* http_req = lisa_http_init(http_param);
	if (!http_req) {
		LISA_LOGE(TAG, "http init error");
		lisa_mem_free(url);
		lisa_mem_free(http_param);
		return LISA_FAIL;
	}
	
	lisa_err_t ret = LISA_OK;
	if (lisa_http_perform(http_req) != LISA_HTTP_OK) {
		LISA_LOGE(TAG, "http get request info err..");
		ret = LISA_FAIL;
	}

	lisa_http_cleanup(http_req);
	lisa_mem_free(url);
	lisa_mem_free(http_param);
	return ret;
}

lisa_err_t lisa_get_online_tts_speed(const lisa_evs_t *const handle, const lisa_http_resp_cb cb)
{
	if (!handle || !handle->access_token || !cb) return LISA_FAIL;

	lisa_http_request_t *http_param = NULL;
	http_param = lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	if (!http_param) return LISA_FAIL;

	http_param->method = LISA_HTTP_GET;
	unsigned char* url = (unsigned char*)lisa_mem_alloc(strlen(URL_GET_OL_TTS_SPEED) + 1);
	if (!url) {
		lisa_mem_free(http_param);
		return LISA_FAIL;
	}
	strcpy(url, URL_GET_OL_TTS_SPEED);
	http_param->url = url;
	http_param->headers = (void *)get_default_headers;
	http_param->timeout = HTTP_TIME_OUT;
	http_param->body_len = 0;
	http_param->body = NULL;
	s_access_token = handle->access_token;
	http_param->user = cb;
	http_param->on_data = (void *)get_evaluate_data_cb;

	lisa_http_t* http_req = lisa_http_init(http_param);
	if (!http_req) {
		LISA_LOGE(TAG, "http init error");
		lisa_mem_free(url);
		lisa_mem_free(http_param);
		return LISA_FAIL;
	}
	
	lisa_err_t ret = LISA_OK;
	if (lisa_http_perform(http_req) != LISA_HTTP_OK) {
		LISA_LOGE(TAG, "http get request info err..");
		ret = LISA_FAIL;
	}

	lisa_http_cleanup(http_req);
	lisa_mem_free(url);
	lisa_mem_free(http_param);
	return ret;
}

lisa_err_t lisa_set_online_tts_speed(const lisa_evs_t *const handle, const uint8_t *const lang, const uint8_t *const speed, const lisa_http_resp_cb cb)
{
	if (!handle || !handle->access_token || !cb || !lang || !speed) return LISA_FAIL;

	lisa_http_request_t *http_param = NULL;
	http_param = lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	if (!http_param) return LISA_FAIL;

	http_param->method = LISA_HTTP_POST;
	unsigned char* url = (unsigned char*)lisa_mem_alloc(strlen(URL_SET_OL_TTS_SPEED) + 1);
	if (!url) {
		lisa_mem_free(http_param);
		return LISA_FAIL;
	}
	strcpy(url, URL_SET_OL_TTS_SPEED);
	http_param->url = url;
	http_param->headers = (void *)get_default_headers;
	http_param->timeout = HTTP_TIME_OUT;
	cJSON *json = cJSON_CreateObject();
	cJSON_AddStringToObject(json, "language", lang);
	cJSON_AddStringToObject(json, "speed", speed);
	char* req_body = cJSON_Print(json);

	http_param->body_len = strlen(req_body);
	http_param->body = req_body;

	s_access_token = handle->access_token;
	http_param->user = cb;
	http_param->on_data = (void *)get_evaluate_data_cb;

	lisa_http_t* http_req = lisa_http_init(http_param);
	if (!http_req) {
		LISA_LOGE(TAG, "http init error");
		lisa_mem_free(url);
		lisa_mem_free(http_param);
		cJSON_free(req_body);
		cJSON_Delete(json);
		return LISA_FAIL;
	}
	
	lisa_err_t ret = LISA_OK;
	if (lisa_http_perform(http_req) != LISA_HTTP_OK) {
		LISA_LOGE(TAG, "http get request info err..");
		ret = LISA_FAIL;
	}

	lisa_http_cleanup(http_req);
	cJSON_free(req_body);
	cJSON_Delete(json);
	lisa_mem_free(url);
	lisa_mem_free(http_param);
	return ret;
}
