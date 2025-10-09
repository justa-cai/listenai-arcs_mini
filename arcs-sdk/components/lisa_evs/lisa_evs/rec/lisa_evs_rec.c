#include "lisa_evs_rec.h"

#include "lisa_mem.h"
#include "lisa_mutex.h"
#include "lisa_queue.h"
#include "lisa_thread.h"
#include "lisa_log.h"
#include "cJSON.h"
#include "lisa_evs.h"
#include "lisa_evs_audiomgr.h"
#include "utils/evs_utils.h"
#include "sockets/lisa_evs_websocket.h"
#include "lisa_typedef.h"

#define TAG "lisa_rec"


/**
 * @brief 音频块
 */
typedef struct rec_audio_block_s {
	char *data;
	int len;
} rec_audio_block_t;

#define REC_QUEUE_WAIT_FOREVER LISA_OS_WAIT_FOREVER

static bool g_debug = false;

lisa_evs_rec_t *lisa_evs_rec_create(lisa_evs_t *evs, lisa_evs_ws_t *ws)
{
	LISA_LOGD(TAG, "lisa evs recognizer create [START]");
	lisa_evs_rec_t *rec = (lisa_evs_rec_t *)lisa_mem_calloc(1, sizeof(lisa_evs_rec_t));

	if (!rec) {
		LISA_LOGE(TAG, "alloc lisa_evs_rec pointer error.");
		goto LISA_EVS_REC_EXIT_ERROR;
	}
	rec->ws = ws;
	rec->evs = evs;

	goto LISA_EVS_REC_EXIT_SUCCESS;

LISA_EVS_REC_EXIT_ERROR:
	rec = NULL;
LISA_EVS_REC_EXIT_SUCCESS:
	LISA_LOGD(TAG, "lisa evs recognizer create [END]");
	return rec;
}

lisa_err_t lisa_evs_rec_audio_start(const lisa_evs_rec_t *const handle, const uint8_t *const req_id, const lisa_evs_rec_audio_param_t *const param)
{
	LISA_LOGD(TAG, "lisa evs rec audio start with request id: %s [BEGIN]", req_id);
	int ret = 0;
	cJSON *root = cJSON_CreateObject();
	cJSON *header = cJSON_CreateObject();
	cJSON *payload = cJSON_CreateObject();
	cJSON_AddStringToObject(header, "name", "recognizer.audio_in");
	cJSON_AddStringToObject(header, "request_id", req_id);
	cJSON_AddItemToObject(root, "header", header);

	cJSON_AddStringToObject(payload, "profile", "CLOSE_TALK");
	switch (param->format) {
		case EVS_REC_AUDIO_FORMAT_BASE: cJSON_AddStringToObject(payload, "format", "AUDIO_L16_RATE_16000_CHANNELS_1"); break;
		case EVS_REC_AUDIO_FORMAT_OPUS: cJSON_AddStringToObject(payload, "format", "OPUS"); break;
		case EVS_REC_AUDIO_FORMAT_SPEEX: cJSON_AddStringToObject(payload, "format", "SPEEX_WB_QUALITY_9"); break;
	}
	cJSON_AddBoolToObject(payload, "enable_vad", param->enable_vad);
	cJSON_AddBoolToObject(payload, "translation", param->translation);
	cJSON_AddNumberToObject(payload, "vad_eos", ((param->vad_eos < 800)? 800:(param->vad_eos)));
	cJSON_AddStringToObject(payload, "reply_key", param->reply_key);
	cJSON_AddItemToObject(root, "payload", payload);
	ret = lisa_evs_client_request(handle->evs, root, req_id);
	if (ret != 0) {
		LISA_LOGE(TAG, "request error!");
		return LISA_FAIL;
	}

	bool speex_enable = (param->format == EVS_REC_AUDIO_FORMAT_SPEEX) ? true : false;
	lisa_evs_websocket_begin_audio(handle->evs->ws, speex_enable);
	LISA_LOGD(TAG, "lisa evs rec audio start [END]");
	return LISA_OK;
}

lisa_err_t lisa_evs_rec_audio_end(const lisa_evs_rec_t *const handle, const uint8_t *const req_id)
{
	lisa_evs_websocket_end_audio(handle->evs->ws);
	return lisa_evs_websocket_send_text(handle->evs->ws, "__END__", strlen("__END__"), NULL);
}

lisa_err_t lisa_evs_rec_audio_stop(const lisa_evs_rec_t *const handle)
{
	lisa_evs_websocket_end_audio(handle->evs->ws);
	return lisa_evs_websocket_send_text(handle->evs->ws, "__END__", strlen("__END__"), NULL);
}

lisa_err_t lisa_evs_rec_audio_cancel(const lisa_evs_rec_t *const handle)
{
	lisa_evs_websocket_end_audio(handle->evs->ws);
	return lisa_evs_websocket_send_text(handle->evs->ws, "__CANCEL__", strlen("__CANCEL__"), NULL);
}

lisa_err_t lisa_evs_rec_send_audio(const lisa_evs_rec_t *const handle, const uint8_t *const audio, int len)
{
	lisa_evs_websocket_send_audio(handle->evs->ws, audio, len);
	return LISA_OK;
}

lisa_err_t lisa_evs_rec_destory(lisa_evs_rec_t *handle)
{
	LISA_LOGD(TAG, "lisa evs recognizer destory [START]");
	if (handle) {
		if (handle->ctx) {
			lisa_mem_free(handle->ctx);
		}

		lisa_mem_free(handle);
	}
	LISA_LOGD(TAG, "lisa evs recognizer destory [END]");
	return LISA_OK;
}

lisa_err_t lisa_evs_rec_evaluate_start(
		const lisa_evs_rec_t *const handle, const uint8_t *const req_id, 
		const lisa_evs_rec_audio_param_t *const param, const lisa_evs_rec_evaluate_param_t *const evaluate)
{
	LISA_LOGD(TAG, "lisa evs rec evaluate start with request id: %s [BEGIN]", req_id);
	int ret = 0;
	cJSON *root = cJSON_CreateObject();
	cJSON *header = cJSON_CreateObject();
	cJSON *payload = cJSON_CreateObject();
	cJSON *evaluate_obj = cJSON_CreateObject();
	cJSON_AddStringToObject(header, "name", "recognizer.audio_in");
	cJSON_AddStringToObject(header, "request_id", req_id);
	cJSON_AddItemToObject(root, "header", header);

	cJSON_AddStringToObject(payload, "profile", "EVALUATE");
	switch (param->format) {
		case EVS_REC_AUDIO_FORMAT_BASE: cJSON_AddStringToObject(payload, "format", "AUDIO_L16_RATE_16000_CHANNELS_1"); break;
		case EVS_REC_AUDIO_FORMAT_OPUS: cJSON_AddStringToObject(payload, "format", "OPUS"); break;
		case EVS_REC_AUDIO_FORMAT_SPEEX: cJSON_AddStringToObject(payload, "format", "SPEEX_WB_QUALITY_9"); break;
	}
	cJSON_AddBoolToObject(payload, "enable_vad", param->enable_vad);
	cJSON_AddNumberToObject(payload, "vad_eos", ((param->vad_eos < 0)? 800:(param->vad_eos)));
	cJSON_AddStringToObject(evaluate_obj, "text", evaluate->text);
	switch (evaluate->category) {
		case EVS_REC_EVALUATE_READ_CHAPTER: cJSON_AddStringToObject(evaluate_obj, "category", "read_chapter"); break;
		case EVS_REC_EVALUATE_READ_SENTENCE: cJSON_AddStringToObject(evaluate_obj, "category", "read_sentence"); break;
		case EVS_REC_EVALUATE_READ_WORD: cJSON_AddStringToObject(evaluate_obj, "category", "read_word"); break;
		case EVS_REC_EVALUATE_READ_SYLLABLE: cJSON_AddStringToObject(evaluate_obj, "category", "read_syllable"); break;
		case EVS_REC_EVALUATE_READ_CHOICE: cJSON_AddStringToObject(evaluate_obj, "category", "read_choice"); break;
	}
	switch (evaluate->language)
	{
		case EVS_REC_EVALUATE_ZH_CN: cJSON_AddStringToObject(evaluate_obj, "language", "zh_cn"); break;
		case EVS_REC_EVALUATE_EN_US: cJSON_AddStringToObject(evaluate_obj, "language", "en_us"); break;
	}
	cJSON_AddItemToObject(payload, "evaluate", evaluate_obj);
	cJSON_AddItemToObject(root, "payload", payload);

	ret = lisa_evs_client_request(handle->evs, root, req_id);
	if (ret != 0) {
		LISA_LOGE(TAG, "request error!");
		return LISA_FAIL;
	}

	bool speex_enable = (param->format == EVS_REC_AUDIO_FORMAT_SPEEX) ? true : false;
	lisa_evs_websocket_begin_audio(handle->evs->ws, speex_enable);
	LISA_LOGD(TAG, "lisa evs rec audio start [END]");
	return LISA_OK;
}

lisa_err_t lisa_evs_rec_evaluate_end(const lisa_evs_rec_t *const handle, const uint8_t *const req_id)
{
	lisa_evs_websocket_end_audio(handle->evs->ws);
	return lisa_evs_websocket_send_text(handle->evs->ws, "__END__", strlen("__END__"), NULL);
}

lisa_err_t lisa_evs_rec_evaluate_cancel(const lisa_evs_rec_t *const handle, const uint8_t *const req_id)
{
	lisa_evs_websocket_end_audio(handle->evs->ws);
	return lisa_evs_websocket_send_text(handle->evs->ws, "__CANCEL__", strlen("__CANCEL__"), NULL);
}

lisa_err_t lisa_evs_rec_translate_start(
		const lisa_evs_rec_t *const handle, const uint8_t *const req_id,
		const lisa_evs_rec_translation_param_t *const param)
{
	LISA_LOGD(TAG, "lisa evs rec translate start with request id: %s [BEGIN]", req_id);

	int ret = 0;
	cJSON *root = cJSON_CreateObject();
	cJSON *header = cJSON_CreateObject();
	cJSON *payload = cJSON_CreateObject();
	cJSON *translate_obj = cJSON_CreateObject();

	cJSON_AddStringToObject(header, "name", "recognizer.text_in");
	cJSON_AddStringToObject(header, "request_id", req_id);
	cJSON_AddItemToObject(root, "header", header);

	cJSON_AddStringToObject(payload, "query", param->text);
	cJSON_AddBoolToObject(payload, "with_tts", param->with_tts);
	cJSON_AddBoolToObject(payload, "translation", param->translation);
	cJSON_AddItemToObject(root, "payload", payload);

	ret = lisa_evs_client_request(handle->evs, root, req_id);
	if (ret != 0) {
		LISA_LOGE(TAG, "request error!");
		return LISA_FAIL;
	}

	LISA_LOGD(TAG, "lisa evs rec translate start [END]");
	return LISA_OK;
}

lisa_err_t lisa_evs_rec_translation_cancel(const lisa_evs_rec_t *const handle)
{
	return lisa_evs_websocket_send_text(handle->evs->ws, "__CANCEL__", strlen("__CANCEL__"), NULL);
}

lisa_err_t lisa_evs_rec_tts_start(
	const lisa_evs_rec_t *const handle, const uint8_t *const req_id,
	const lisa_evs_rec_tts_param_t *const param)
{
LISA_LOGD(TAG, "lisa evs rec tts start with request id: %s [BEGIN]", req_id);

int ret = 0;
cJSON *root = cJSON_CreateObject();
cJSON *header = cJSON_CreateObject();
cJSON *payload = cJSON_CreateObject();
cJSON *translate_obj = cJSON_CreateObject();

cJSON_AddStringToObject(header, "name", "audio_player.tts.text_in");
cJSON_AddStringToObject(header, "request_id", req_id);
cJSON_AddItemToObject(root, "header", header);

cJSON_AddStringToObject(payload, "text", param->text);
cJSON_AddNumberToObject(payload, "speed", param->speed);
cJSON_AddNumberToObject(payload, "volume", param->volume);
cJSON_AddStringToObject(payload, "vcn", param->vcn);
cJSON_AddItemToObject(root, "payload", payload);

ret = lisa_evs_client_request(handle->evs, root, req_id);
if (ret != 0) {
	LISA_LOGE(TAG, "request error!");
	return LISA_FAIL;
}

LISA_LOGD(TAG, "lisa evs rec tts start [END]");
return LISA_OK;
}

lisa_err_t lisa_evs_rec_tts_cancel(const lisa_evs_rec_t *const handle)
{
return lisa_evs_websocket_send_text(handle->evs->ws, "__CANCEL__", strlen("__CANCEL__"), NULL);
}
