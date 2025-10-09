#include "lisa_evs_app.h"
#include "lisa_evs_config.h"
#include "product_config.h"
#include "lisa_log.h"
#include "lisa_evs.h"
#include "evs_auth_mgr.h"
#include <string.h>
#include "lisa_thread.h"
#include "lisa_queue.h"
#include "lisa_semaphore.h"
#include "lisa_mem.h"
#include "lisa_typedef.h"
#include "utils/evs_utils.h"
#include "lisa_evs_uuid.h"
#include "lite_adc.h"
#include "lisa_http.h"
#include "playmp3.h"
#include "HTTPCUsr_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lsfs.h"
#include "low_play.h"

#define TAG "lisa_evs_app"

#define LOOP_AUDIO_TEST_SIZE (REC_STEP_SAMPS * 2)
#define REC_STEP_SAMPS  (256)

#define PLAY_QUE_BUF_SIZE (1152)

typedef aud_samp_t adc_step_t[REC_STEP_SAMPS];

typedef struct {
	int len;
	int pos;
} download_info_t;

typedef enum {
    EVS_AUDIO_EVALUATE_STOPPED,
    EVS_AUDIO_EVALUATE_RUNNING
} evs_audio_evaluate_state_t;

static evs_audio_evaluate_state_t evaluate_state = EVS_AUDIO_EVALUATE_STOPPED;

static char g_request_access_token[80] = {0};
static uint8_t g_request_id[40] = {0};
static lisa_evs_t *g_handle = NULL;

static bool g_ws_connected = false;
static struct lsfs_file_t record_file;

static lisa_evs_context_t *g_lisa_evs_ctx = NULL;

download_info_t s_info = {.len = 0, .pos = -1 };

/**
 * @brief 初始化iflyos设备账号信息
 * 
 */
lisa_evs_config_t g_lisa_evs_config = {
    .client_id = PRODUCT_CLIENT_ID,
    .device_id = PRODUCT_DEVICE_ID,
    .ota_secret = PRODUCT_OTA_SECRET_ID
};

void lisa_evs_translate_result_handle(const uint8_t *const msg, uint32_t len)
{
	cJSON *json = NULL;
	uint8_t *result = NULL;
	uint32_t size = 0;
	trans_err_e trans_err = TRANS_ERROR;

	json = cJSON_Parse(msg);
	if (json == NULL) {
		LISA_LOGE(TAG, "Failed to parsing trans_result JSON");
		goto err;
	}

	cJSON *data = cJSON_GetObjectItem(json, "data");
	if (data == NULL) {
		LISA_LOGE(TAG, "Failed to parsing translate data JSON");
	}

	cJSON *dst = cJSON_GetObjectItem(data, "dst");
	if (dst == NULL) {
		LISA_LOGE(TAG, "Failed to parsing translate dst JSON");
	}
	result = dst->valuestring;
	size = strlen(dst->valuestring);
	trans_err = TRANS_COMPLETE;

err:

	g_lisa_evs_ctx->cbs.translate_result(result, size, trans_err);
	cJSON_Delete(json);
}

static void __http_on_data(lisa_http_data_t *data)
{
	download_info_t *p = (download_info_t *)data->user;
	p->len += data->len;

	lisa_mp3_send(data->buf, data->len);
	mp3_play_start();

	LISA_LOGD(TAG, "http down len: %d", p->len);
}

static void lisa_evs_tts_play(void *param)
{
	lisa_http_request_t req;

	s_info.len = 0;
	s_info.pos = -1;
	memset(&req, 0, sizeof(lisa_http_request_t));
	req.method = LISA_HTTP_GET;
	req.url = (uint8_t *)param;
	req.timeout = 10;
	req.on_data = __http_on_data;
	req.body = NULL;
	req.body_len = 0;
	req.headers = NULL;
	req.user = &s_info;

	if (req.url == NULL) {
		LISA_LOGE(TAG, "[http] req.url err\n");
		goto exit;
	}

	LISA_LOGI(TAG, "[http] download begin: %s", req.url);

    lisa_http_t *http = lisa_http_init(&req);
	if (!http) {
		LISA_LOGE(TAG, "[http] init err\n");
		goto exit;
	}
	
	if (lisa_http_download(http) == LISA_HTTP_OK) {

	} else {
		LISA_LOGE(TAG, "[http] download failed");
	}
	mp3_play_stop();
	lisa_http_cleanup(http);
	lisa_mem_free((void*)req.url);

exit:
	vTaskDelete(NULL);
}

void lisa_evs_tts_result_handle(const uint8_t *const msg, uint32_t len)
{
	cJSON *json = NULL;
	uint8_t *result = NULL;
	tts_err_e tts_err = TTS_ERROR;

	lisa_thread_attr_t thread_attr;
	thread_attr.name = "lisa_evs_tts_play";
	thread_attr.stack_size = 3 * 1024;
	thread_attr.priority = LISA_OS_PRIORITY_NORMAL;

	json = cJSON_Parse(msg);
	if (json == NULL) {
		LISA_LOGE(TAG, "Failed to parsing tts_result JSON");
		goto err;
	}

	cJSON *secure_url = cJSON_GetObjectItem(json, "secure_url");
	if (secure_url == NULL) {
		LISA_LOGE(TAG, "Failed to parsing tts secure_url JSON");
		goto err;
	}
	tts_err = TTS_COMPLETE;
	uint8_t *url = lisa_mem_alloc(strlen(secure_url->valuestring) + 1);
	if (url == NULL) {
		LISA_LOGE(TAG, "Failed to malloc url");
		goto err;
	}
	memcpy(url, "http", 4);
	memcpy(url + 4, secure_url->valuestring + 5, strlen(secure_url->valuestring) - 5);
	url[strlen(secure_url->valuestring) - 1] = '\0';
	lisa_thread_create(&thread_attr, lisa_evs_tts_play, url);

err:

	g_lisa_evs_ctx->cbs.tts_result(tts_err);
	cJSON_Delete(json);
}

void lisa_evs_tts_play_start(void)
{
	g_lisa_evs_ctx->cbs.tts_result(TTS_PLAY_START);
	static low_play_cfg_t play_cfg = {
		.channel = 1,
		.rate   = 16000,
		.bit = 16,
		.vol = 113, /*TODO:修复底层音量过低的问题*/
		.ap_mode = 0
	};
	low_play_start(&play_cfg);
}

void lisa_evs_tts_play_stop(void)
{
	g_lisa_evs_ctx->cbs.tts_result(TTS_PLAY_END);
	low_play_stop(low_play_get_handle());
}

/**
 * @brief websocket断开重连
 * 
 * @param arg 
 * @return int 
 */
int ws_disconn_runnable(void* arg)
{
	/*如需断开重连，下方做重连动作*/
	lisa_evs_connect(g_handle, g_request_access_token);
	return 0;
}

/**
 * @brief websocket连接成功回调
 * 
 */
void ws_connected_cb()
{
	g_ws_connected = true;
	LISA_LOGI(TAG, "websocket connected\n");
}

/**
 * @brief websocket连接失败回调
 * 
 * @param msg 
 */
void ws_connect_failed_cb(const uint8_t *const msg)
{
	LISA_LOGI(TAG, "websocket connect fail\n");
	g_ws_connected = false;
	evs_handler_post_runnable_delay(ws_disconn_runnable, NULL, 5000);
}

/**
 * @brief websocket断开连接回调
 * 
 * @param msg 
 */
void ws_disconnected_cb(const uint8_t *const msg)
{
	g_ws_connected = false;
	LISA_LOGI(TAG, "websocket disconnect\n");
	evs_handler_post_runnable_delay(ws_disconn_runnable, NULL, 5000);
}

/**
 * @brief websocket消息回调
 * 
 * @param action 
 * @param msg 
 * @param len 
 * @param req_id 
 */
void ws_message_cb(const uint8_t *const action, const uint8_t *const msg, int len, const uint8_t *const req_id)
{
	LISA_LOGD(TAG, "message, action: %s, len: %d, payload: %s, request id: %s", action, len, msg,
			req_id);
 	if (strcmp(action, "recognizer.evaluate_result") == 0) {
		if ((g_lisa_evs_ctx != NULL) && (g_lisa_evs_ctx->cbs.evaluate_result != NULL))
		{
			g_lisa_evs_ctx->cbs.evaluate_result(msg, len);
		}
	} else if (strcmp(action, "recognizer.trans_result") == 0) {
		if ((g_lisa_evs_ctx != NULL) && (g_lisa_evs_ctx->cbs.translate_result != NULL))
		{
			lisa_evs_translate_result_handle(msg, len);
		}
	} else if (strcmp(action, "audio_player.audio_out") == 0) {
		if ((g_lisa_evs_ctx != NULL) && (g_lisa_evs_ctx->cbs.tts_result != NULL))
		{
			lisa_evs_tts_result_handle(msg, len);
		}
	}
}

/**
 * @brief websocket消息发送失败回调
 * 
 * @param type 
 * @param data 
 */
void ws_send_failed_cb(ws_msg_type_e type, const uint8_t *data)
{
	LISA_LOGD(TAG, "websocket send fail\n");
}

/**
 * @brief iflyos 下发ping包
 * 
 * @param msg 
 * @param req_id 
 */
void sys_on_ping(const uint8_t *const msg, const uint8_t *const req_id)
{
	LISA_LOGD(TAG, "sysrem ping, msg: %s, req id: %s", msg, req_id);
}

/**
 * @brief iflyos 下发系统错误消息
 * 
 * @param msg 
 * @param req_id 
 */
void sys_on_error(const uint8_t *const msg, const uint8_t *const req_id)
{
	LISA_LOGD(TAG, "sysrem error, msg: %s, req id: %s", msg, req_id);
}

/**
 * @brief iflyos 下发云端检查版本更新消息
 * 
 * @param msg 
 * @param req_id 
 */
void sys_on_check_software_update(const uint8_t *const msg, const uint8_t *const req_id)
{
	LISA_LOGD(TAG, "check software update, msg: %s, req id: %s", msg, req_id);
}

/**
 * @brief iflyos 下发版本升级消息
 * 
 * @param msg 
 * @param req_id 
 */
void sys_on_update_software(const uint8_t *const msg, const uint8_t *const req_id)
{
	LISA_LOGD(TAG, "software update, msg: %s, req id: %s", msg, req_id);
}

/**
 * @brief iflyos 云端下发撤销鉴权消息
 * 
 * @param msg 
 * @param req_id 
 */
void sys_on_revoke_auth(const uint8_t *const msg, const uint8_t *const req_id)
{
	LISA_LOGD(TAG, "revoke auth, msg: %s, req id: %s", msg, req_id);
}

/**
 * @brief iflyos 云端下发关机消息
 * 
 * @param msg 
 * @param req_id 
 */
void sys_on_power_off(const uint8_t *const msg, const uint8_t *const req_id)
{
	LISA_LOGD(TAG, "power off, msg: %s, req id: %s", msg, req_id);
}

/**
 * @brief iflyos 云端下发音量调节消息
 * 
 * @param msg 
 * @param req_id 
 */
void spk_on_set_vol(const uint8_t *const msg, const uint8_t *const req_id)
{
	LISA_LOGD(TAG, "set vol, msg: %s, req id: %s", msg, req_id);
}

/**
 * @brief 获取设备端状态
 * 
 * @return lisa_evs_state_t 
 */
lisa_evs_state_t evs_get_state()
{
	//需要根据实际的状态来
	LISA_LOGD(TAG, "lisa evs get all state");
	lisa_evs_state_t state = {0};
	// system相关
	strcpy(state.system_state.version, "1.3");
	strcpy(state.system_state.firmware_version, "1.0.1");

	// playback相关
	strcpy(state.playback_state.version, "1.2");
	strcpy(state.playback_state.resource_id, "");
	state.playback_state.state = LISA_EVS_PLAYSTATE_IDLE;
	state.playback_state.offset = 0;
	// speaker相关
	strcpy(state.speaker_state.version, "1.0");
	state.speaker_state.volume = 100;
	// platform相关
	strcpy(state.platform_info.platform_name, "FreeRTOS");
	strcpy(state.platform_info.platform_version, "10.2.1");
	return state;
}

/**
 * @brief 初始化websocket回调接口
 * 
 */
lisa_evs_websocket_cb_t g_ws_cb = {.connected = ws_connected_cb,
    .connect_failed = ws_connect_failed_cb,
    .disconnected = ws_disconnected_cb,
    .message = ws_message_cb,
    .send_failed = ws_send_failed_cb};

/**
* @brief 初始化iflyos系统消息回调接口
* 
*/
lisa_evs_system_cb_t g_sys_cb = {.on_ping = sys_on_ping,
    .on_error = sys_on_error,
    .on_check_software_update = sys_on_check_software_update,
    .on_update_software = sys_on_update_software,
    .on_revoke_auth = sys_on_revoke_auth,
    .on_power_off = sys_on_power_off};

/**
* @brief 初始化iflyos扬声器回调接口
* 
*/
lisa_evs_speaker_cb_t g_spk_cb = {.on_set_vol = spk_on_set_vol};

/**
* @brief 初始化 iflyos websocket、系统消息、扬声器接口回调
* 
*/
lisa_evs_cb_t g_evs_cb = {.websocket_cb = &g_ws_cb,
    .system_cb = &g_sys_cb,
    .speaker_cb = &g_spk_cb};

/**
* @brief 初始化iflyos设备状态获取回调
* 
*/
lisa_evs_state_cb_t g_get_state_cb = {.get_state = evs_get_state};

static bool is_stop_evaluate = false;
static EventGroupHandle_t evaluate_evt_hdl;
#define EVALUATE_EVT_START          (1 << 0)
#define EVALUATE_EVT_PLAY          	(1 << 1)

void run_evs_audio_evaluate(void *param)
{
	int sleep_time = LOOP_AUDIO_TEST_SIZE / 32;
	uint8_t rec_buf[LOOP_AUDIO_TEST_SIZE];
	adc_step_t *xdat = NULL;
	uint32_t size = 0;
	uint8_t play_buf[PLAY_QUE_BUF_SIZE];

	LISA_LOGI(TAG, "send evaluate begin, sleep time %dms", sleep_time);
	
	while(1) 
	{
		EventBits_t evt_bits = xEventGroupWaitBits(evaluate_evt_hdl, 
								EVALUATE_EVT_START | EVALUATE_EVT_PLAY,
								pdFALSE, pdFALSE, portMAX_DELAY);

		if (evt_bits & EVALUATE_EVT_START) {
			int ret = lite_adc_read(&xdat, REC_STEP_SAMPS, LISA_OS_WAIT_FOREVER);
			if(ret != REC_STEP_SAMPS) {
				LISA_LOGE(TAG, "lite_adc_read ret:%d", ret);
			} else {
				memcpy(rec_buf, xdat, ret * sizeof(aud_samp_t));
				lisa_evs_rec_send_audio(g_handle->rec, rec_buf, ret * sizeof(aud_samp_t));
				size = lsfs_write(&record_file, xdat, ret * sizeof(aud_samp_t));
				if (size != ret * sizeof(aud_samp_t))
				{
					LISA_LOGE(TAG, "lsfs_write record file failed size(%d)!=ret(%d)", size, ret * sizeof(aud_samp_t));
				}
			}
	
			if (is_stop_evaluate)
			{
				LISA_LOGI(TAG, "Now End Send Audio");
				xEventGroupClearBits(evaluate_evt_hdl, EVALUATE_EVT_START);
				lite_adc_deinit();
				lisa_evs_rec_evaluate_end(g_handle->rec, g_request_id);
				lsfs_close(&record_file);
				evaluate_state = EVS_AUDIO_EVALUATE_STOPPED;
			}
		}

		if (evt_bits & EVALUATE_EVT_PLAY) {
			size = lsfs_read(&record_file, play_buf, sizeof(play_buf));
			if (size <= 0) {
				xEventGroupClearBits(evaluate_evt_hdl, EVALUATE_EVT_PLAY);
				lsfs_close(&record_file);
				low_play_stop(low_play_get_handle());
			}
			low_play_pcm_write(low_play_get_handle(), (int16_t *)play_buf, size / 2);
		}
	}

	vTaskDelete(NULL);
}

void lisa_evs_audio_evaluate_start(lisa_evs_rec_evaluate_param_t *evaluate_param)
{
	int result;

	if (evaluate_state == EVS_AUDIO_EVALUATE_RUNNING) {
        LISA_LOGE(TAG, "Evs evaluate is already running.");
        return;
    }

    evaluate_state = EVS_AUDIO_EVALUATE_RUNNING;
	
	is_stop_evaluate = false;

	lisa_evs_uuid_generate_string(g_request_id);
	lisa_evs_rec_audio_param_t audio_param;
	audio_param.format = EVS_REC_AUDIO_FORMAT_BASE;
	audio_param.profile = EVS_REC_AUDIO_PROFILE_CLOSE_TALK;
	audio_param.enable_vad = true;
	audio_param.vad_eos = 300;

    lsfs_unlink(EVS_EVALUATE_RECORD_FILE);
    lsfs_file_t_init(&record_file);
    result = lsfs_open(&record_file, EVS_EVALUATE_RECORD_FILE, LSFS_O_CREATE | LSFS_O_TRUNC | LSFS_O_WRITE);
    if (result != 0) {
        LISA_LOGE(TAG, "Failed to open file for evaluate record: %d", result);
        return;
    }

	result = lite_adc_init();
	if (result != 0) {
		LISA_LOGE(TAG, "Failed to init adc: %d", result);
        return;
	}

    lite_adc_ctrl(MAPI_AADC_CTRL_REC_START, NULL);

	xEventGroupSetBits(evaluate_evt_hdl, EVALUATE_EVT_START);

	lisa_evs_rec_evaluate_start(g_handle->rec, g_request_id, &audio_param, evaluate_param);
}

void lisa_evs_audio_evaluate_stop(void)
{
	LISA_LOGI(TAG, "lisa_evs_audio_evaluate_stop");
	is_stop_evaluate = true;
}

void lisa_evs_audio_evaluate_play(void)
{
	int result;

	result = lsfs_open(&record_file, EVS_EVALUATE_RECORD_FILE, LSFS_O_READ);
    if (result != 0) {
        LISA_LOGE(TAG, "Failed to open file for evaluate record: %d", result);
        return;
    }

	low_play_cfg_t play_cfg = {
		.channel = 1,
		.rate   = 16000,
		.bit = 16,
		.vol = 113, /*TODO:修复底层音量过低的问题*/
		.ap_mode = 0
	};
	low_play_start(&play_cfg);

	xEventGroupSetBits(evaluate_evt_hdl, EVALUATE_EVT_PLAY);
}

/**
 * @brief 进行文本翻译
 * 
 */
void run_evs_text_translate(uint8_t *text)
{
	lisa_evs_uuid_generate_string(g_request_id);
	lisa_evs_rec_translation_param_t param;

	param.text = text;
	param.with_tts = false;
	param.translation = true;
	lisa_evs_rec_translate_start(g_handle->rec, g_request_id, &param);
}

/**
 * @brief 进行tts
 * 
 */
void run_evs_tts(uint8_t *text)
{
	lisa_evs_uuid_generate_string(g_request_id);
	lisa_evs_rec_tts_param_t param;

	param.text = text;
	param.speed = 2.08;
	param.volume = 20;
	param.vcn = "x2_yezi";

	lisa_evs_rec_tts_start(g_handle->rec, g_request_id, &param);
}

bool is_ws_connect(void)
{
	lisa_evs_ws_t *ws = NULL;

	if (g_handle == NULL)
		return false;

	ws = g_handle->ws;

	if (ws != NULL && ws->m_context != NULL)
	{
		ws_connect_status_e statue = ws->m_context->m_connected;
		if (statue == WS_STATUS_CONNECTED) return true;
		return false;
	}
	else
	{
		LISA_LOGI(TAG, "ws: %p is NUll\n", ws);
	}

	return false;
}

/**
 * @brief 设备端初始化
 * 
 */
int lisa_evs_auth()
{	
    int ret; 
	
	// 连上网以后进行EVS鉴权操作
    ret = start_evs_auth(&g_lisa_evs_config);
	if (ret != 0)
	{
		return ret;
	}
    
    const char *accessToken = get_evs_access_token();
	if (strlen(accessToken) == 0)
	{
		return -1;
	}
    LISA_LOGD(TAG, "accessToken : %s", accessToken);
    strcpy(g_request_access_token, accessToken);
    g_handle = lisa_evs_create(&g_lisa_evs_config, &g_evs_cb, &g_get_state_cb);
	if (g_handle == NULL) {
		LISA_LOGE(TAG, "Failed to creat esv handle");
		return -1;
	}
    // 4、设置access token
    ret = lisa_evs_set_access_token(g_handle, g_request_access_token, strlen(g_request_access_token));
	if (ret != 0) {
		lisa_evs_destroy(g_handle);
		g_handle = NULL;
		return -1;
	}
}

/**
 * @brief 连接iflyos
 * 
 */
void connect_iflyos()
{
	lisa_err_t ret = 0;
	static bool is_run_connect = false;
	
	if (is_ws_connect())
	{
		LISA_LOGD(TAG, "ws is already connect!\n");
	}
	else
	{
		if (g_handle == NULL)
		{
			lisa_evs_auth();
		}
		
		if ((g_handle != NULL) && (is_run_connect == false))
		{
			ret = lisa_evs_connect(g_handle, g_request_access_token);
			if (ret == 0)
				is_run_connect = true;
		}
	}
}

void lisa_evs_init_thread(void* arg)
{
	while (1)
	{
		extern bool g_wifi_connected;
		if (g_wifi_connected) {
			connect_iflyos();
		}

		lisa_thread_mdelay(2000);
	}
}

lisa_evs_context_t *lisa_evs_init(lisa_evs_cbs_t *cbs)
{
	lisa_thread_attr_t thread_attr;
	lisa_mp3_cbs_t mp3_cbs;
	lisa_thread_t *evaluate_thread = NULL;

	if (NULL != g_lisa_evs_ctx) {
        return g_lisa_evs_ctx;
    }

	g_lisa_evs_ctx = lisa_mem_alloc(sizeof(lisa_evs_context_t));
	if (g_lisa_evs_ctx == NULL) {
        LISA_LOGE(TAG, "Failed to memory g_lisa_evs_ctx: (%zu bytes)", sizeof(lisa_evs_context_t));
        return NULL;
    }
	memset(g_lisa_evs_ctx, 0, sizeof(lisa_evs_context_t));

	if (cbs != NULL) {
        memcpy(&g_lisa_evs_ctx->cbs, cbs, sizeof(lisa_evs_cbs_t));
    }

	mp3_cbs.lisa_play_start = lisa_evs_tts_play_start;
	mp3_cbs.lisa_play_stop = lisa_evs_tts_play_stop;
	mp3_play_init(&mp3_cbs);

	g_lisa_evs_ctx->ops.is_ws_connect = is_ws_connect;
	g_lisa_evs_ctx->ops.evaluate_start = lisa_evs_audio_evaluate_start;
	g_lisa_evs_ctx->ops.evaluate_stop = lisa_evs_audio_evaluate_stop;
	g_lisa_evs_ctx->ops.evaluate_play = lisa_evs_audio_evaluate_play;
	g_lisa_evs_ctx->ops.translate_start = run_evs_text_translate;
	g_lisa_evs_ctx->ops.tts_start = run_evs_tts;
	
	thread_attr.name = "lisa_evs_init_thread";
	thread_attr.stack_size = 2 * 1024;
	thread_attr.priority = LISA_OS_PRIORITY_NORMAL;

	evs_utils_init();
	lisa_thread_create(&thread_attr, lisa_evs_init_thread, NULL);

	evaluate_evt_hdl = xEventGroupCreate();
	lisa_thread_attr_t evaluate_thread_attr;
	evaluate_thread_attr.name = "evaluate_thread";
	evaluate_thread_attr.stack_size = 5 * 1024;
	evaluate_thread_attr.priority = LISA_OS_PRIORITY_NORMAL;  // LISA_OS_PRIORITY_NORMAL

	// 评测接口
	evaluate_thread = lisa_thread_create(&evaluate_thread_attr, run_evs_audio_evaluate, NULL);
	if (evaluate_thread == NULL) {
		LISA_LOGE(TAG, "evaluate_thread create failed");
		return NULL;
	}

	LISA_LOGI(TAG, "Create lisa evs ctx success!");

	return g_lisa_evs_ctx;
}
