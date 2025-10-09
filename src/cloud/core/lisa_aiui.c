#define TAG "lisa_aiui"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lisa_aiui.h"
#include "lisa_aiui_ws.h"
#include "aiui_base64.h"
#include "aiui_cfg.h"
#include "lisa_log.h"
#include "lisa_websocket.h"
#include "lisa_http.h"
#include "lisa_mem.h"
#include "mbedtls/md5.h"
#include "lwip/sockets.h"
// #include "datetime.h"

#include <FreeRTOS.h>
#include "queue.h"
#include "semphr.h"
#include "cJSON.h"
// #include "evs_pref.h"
#include "lisa_typedef.h"
#include "lisa_kv.h"
#include "kv.h"
#include "lisa_time.h"
#include "lisa_aiui_rid_man.h"

#define AIUI_TIMEOUT (10)

#define AIUI_PORT "80"
#define AIUI_SCHEME "ws"

#define AIUI_HOST                   "api.listenai.com"
#define AIUI_TOKEN_URL              "http://api.listenai.com/v1/auth/tokens"

#define AIUI_HOST_STAGING           "staging-api.listenai.com"
#define AIUI_TOKEN_URL_STAGING      "http://staging-api.listenai.com/v1/auth/tokens"


lisa_aiui_t *s_lisa_aiui = NULL;
#define DEFAULT_INTERACTIVE_MODE    INTER_ONESHOT
static lisa_aiui_interactive_mode_e current_intera_mode = DEFAULT_INTERACTIVE_MODE;  //default mode
static bool is_cur_inter_mode_valid = false;

static QueueHandle_t token_queue = NULL;
static SemaphoreHandle_t ota_sema = NULL; 

static uint8_t id_buffer[8] = {0x0A, 0x0B, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
static char id_buffer_str[sizeof(id_buffer) * 2 + 1] = {'\0'};
void update_device_id(void);

const char *get_device_id_str(void)
{
	update_device_id();
	return &id_buffer_str[0];
}

//0 --> oneshot  1 --> continue
lisa_aiui_interactive_mode_e lisa_aiui_get_interactive_mode(void)
{
    return current_intera_mode;
}

int lisa_aiui_set_interactive_mode(lisa_aiui_interactive_mode_e mode)
{
    if (current_intera_mode == mode) {
        return 0;
    }

	int save_mode;
	int r;

	r = lisa_kv_get_int(KV_KEY_INTERACTIVE_MODE, &save_mode);
	if (r != 0 || save_mode != mode) {
		r = lisa_kv_set_int(KV_KEY_INTERACTIVE_MODE, mode);
	}

	if (r != 0) {
		LISA_LOGE(TAG, "save interactive mode failed");
		return -1;
	}

	current_intera_mode = mode;

    return 0;
}

bool lisa_aiui_is_staging_mode_enable(void)
{
	int r;
	int staging_mode = 0;

	r = lisa_kv_get_int(KV_KEY_STAGING, &staging_mode);
	if (r != 0) {
		staging_mode = 0;
	}

	LISA_LOGI(TAG, "staging mode: %d", staging_mode);

	if(staging_mode == 1) {
		return true;
	}
	return false;
}

void update_device_id(void)
{
    uint32_t *id_1 = (uint32_t *)0x48600208;
    uint32_t *id_2 = (uint32_t *)0x4860020c;
	char *device_id = NULL;
	int r;

	r = lisa_kv_get_string(KV_KEY_USER_DEVICE_ID, &device_id);
	if (r == 0 && device_id != NULL) {
		LISA_LOGW(TAG, "get device id from kv success");
		if (strlen(device_id) >= 16) {
            memcpy(id_buffer_str, device_id, 16);
        } else {
            memcpy(id_buffer_str, device_id, strlen(device_id));
        }
		lisa_kv_free(device_id);
		return;
	}

    if (*id_1 == 0 && *id_2 == 0) {
		LISA_LOGW(TAG, "chip id is all zero");
    } else {
		LISA_LOGW(TAG, "chip id is valid");
        memcpy(id_buffer, id_1, sizeof(uint32_t));
        memcpy(id_buffer + 4, id_2, sizeof(uint32_t));
    }

    sprintf(id_buffer_str, "%02x%02x%02x%02x%02x%02x%02x%02x", id_buffer[0], id_buffer[1], id_buffer[2], id_buffer[3],
            id_buffer[4], id_buffer[5], id_buffer[6], id_buffer[7]);
}

lisa_aiui_t *lisa_aiui_create(const lisa_aiui_config_t *const config, lisa_aiui_cb_t *aiui_cb)
{
    update_device_id();
	lisa_aiui_rid_man_init();

	LISA_LOGD(TAG, "lisa aiui create. [START]");

	if (!config) {
		LISA_LOGE(TAG, "config pointer is null.");
		return NULL;
	}
	s_lisa_aiui = (lisa_aiui_t *)lisa_mem_calloc(1, sizeof(lisa_aiui_t));
	if (!s_lisa_aiui) {
		LISA_LOGE(TAG, "alloc lisa_aiui pointer error.");
		goto LISA_AIUI_EXIT_ERROR;
	}
	s_lisa_aiui->aiui_ws = (lisa_aiui_ws_t *)lisa_mem_calloc(1, sizeof(lisa_aiui_ws_t));

	s_lisa_aiui->aiui_ws_connected = aiui_cb->aiui_ws_connected_cb;
	s_lisa_aiui->aiui_ws_onmessage = aiui_cb->aiui_ws_onmessage_cb;
	s_lisa_aiui->aiui_ws_disconnect = aiui_cb->aiui_ws_disconnect_cb;

	lisa_aiui_config_t *aiui_config = NULL;
	uint8_t appid_len = 0;
	uint8_t api_key_len = 0;
	uint8_t api_secret_len = 0;
	uint8_t scene_len = 0;

	// alloc config
	aiui_config = (lisa_aiui_config_t *)lisa_mem_alloc(sizeof(lisa_aiui_config_t));
	if (!aiui_config) {
		LISA_LOGE(TAG, "alloc config pointer error.");
		goto LISA_AIUI_EXIT_ERROR_CONFIG;
	}

    token_queue = xQueueCreate(10, sizeof(char *));
    if (token_queue == NULL) {
        LISA_LOGE(TAG, "---- no mem");
        goto LISA_AIUI_EXIT_ERROR_APPID;
    }

	// appid
	appid_len = strlen(config->appid);
	aiui_config->appid = (char *)lisa_mem_alloc(sizeof(char) * (appid_len + 1));
	if (!aiui_config->appid) {
		LISA_LOGE(TAG, "alloc appid pointer error.");
		goto LISA_AIUI_EXIT_ERROR_APPID;
	}
	memcpy(aiui_config->appid, config->appid, appid_len);
	aiui_config->appid[appid_len] = '\0';

	// apikey
	api_key_len = strlen(config->api_key);
	aiui_config->api_key = (char *)lisa_mem_alloc(sizeof(char) * (api_key_len + 1));
	if (!aiui_config->api_key) {
		LISA_LOGE(TAG, "alloc api_key pointer error.");
		goto LISA_AIUI_EXIT_ERROR_API_KEY;
	}
	memcpy(aiui_config->api_key, config->api_key, api_key_len);
	aiui_config->api_key[api_key_len] = '\0';

	s_lisa_aiui->config = aiui_config;

	if (lisa_kv_get_int(KV_KEY_INTERACTIVE_MODE, (int *)&current_intera_mode) != 0) {
		LISA_LOGI(TAG, "get interactive mode failed, use default mode: %d", DEFAULT_INTERACTIVE_MODE);
		current_intera_mode = DEFAULT_INTERACTIVE_MODE;
	} else {
		LISA_LOGI(TAG, "interactive mode: %d", current_intera_mode);
	}

	goto LISA_AIUI_EXIT_SUCCESS;

LISA_AIUI_EXIT_ERROR_API_KEY:
	lisa_mem_free(aiui_config->appid);
LISA_AIUI_EXIT_ERROR_APPID:
	lisa_mem_free(aiui_config);
LISA_AIUI_EXIT_ERROR_CONFIG:
	lisa_mem_free(s_lisa_aiui->aiui_ws);
	lisa_mem_free(s_lisa_aiui);
LISA_AIUI_EXIT_ERROR:
	s_lisa_aiui = NULL;
LISA_AIUI_EXIT_SUCCESS:
	LISA_LOGD(TAG, "lisa aiui create sucess");

	return s_lisa_aiui;
}

static void get_ws_data_cb(lisa_ws_data_t *data)
{
	if (s_lisa_aiui) {
		s_lisa_aiui->aiui_ws_onmessage((const char *)(data->buf), data->len);
	} else {
		LISA_LOGE(TAG, "aiui_ws_disconnect fail, beacase null pointer.");
	}
}

static void get_ws_event_cb(lisa_ws_event_t *event)
{
	if (event->what == LISA_WS_ON_CONNECTED) {
        LISA_LOGI(TAG, "lisa ws connect.");
		if (s_lisa_aiui) {
			s_lisa_aiui->aiui_ws_connected();
		} else {
			LISA_LOGE(TAG, "aiui_ws_connect fail, beacaseof null pointer.");
		}

	} else if (event->what == LISA_WS_ON_DISCONNECTED) {
        LISA_LOGI(TAG, "lisa ws disconnect.");
		if (s_lisa_aiui) {
			s_lisa_aiui->aiui_ws_disconnect();
			// lisa_ws_cleanup(s_lisa_aiui->aiui_ws->ws_client);
		} else {
            LISA_LOGE(TAG, "aiui_ws_disconnect fail, beacaseof null pointer.");
		}
	} else {
		LISA_LOGE(TAG, "event %d", event->what);
	}
}

static void http_aiui_token_on_data(lisa_http_data_t *data)
{
	LISA_LOGI(TAG, "http_aiui_token_on_data, %s", (char *)data->buf);
    cJSON *json_root = cJSON_Parse(data->buf);
    if (!json_root) {
        LISA_LOGE(TAG, "aiui token json parse failed\n");
        return;
    }
    cJSON *cj_token = cJSON_GetObjectItem(json_root, "token");
    if (cj_token == NULL) {
        LISA_LOGE(TAG, "parse token failed\n");
        return;
    }
    int token_len = strlen(cj_token->valuestring);
    char *aiui_token = lisa_mem_calloc(1, token_len + 1);
    if (aiui_token == NULL) {
        LISA_LOGE(TAG, "has no mem\n");
        return;
    }
    memcpy(aiui_token, cj_token->valuestring, token_len);

    //send the aiui string to update token api
    xQueueSend(token_queue, &aiui_token, portMAX_DELAY);
    cJSON_Delete(json_root);
}

static void* http_client_get_headers(void)
{
#define HTTP_REQ_HEADER "Content-Type: application/json"
    return HTTP_REQ_HEADER;
}

static char * aiui_generate_token(const char *product_id, const char *secret_id, const char *device_id)
{
    int ret = 0;
    char current_time[12] = {0};
    struct timeval tv;
	gettimeofday(&tv, NULL);
    sprintf(current_time, "%lld", tv.tv_sec);
    LISA_LOGI(TAG, "system time: %s", current_time);
	const char *dev_id = get_device_id_str();
	uint32_t device_id_len = strlen(dev_id);

    char *origin_product_ntp = lisa_mem_calloc(1, strlen(secret_id)+strlen(current_time) + device_id_len + 1);
    if (origin_product_ntp == NULL) {
        LISA_LOGE(TAG, "malloc failed\n");
        return NULL;
    }
    strcat(origin_product_ntp, secret_id);
    strcat(origin_product_ntp, dev_id);
    strcat(origin_product_ntp, current_time);

    char md5_hex_string[80] = {'\0'};
    unsigned char output[16] = {'\0'};
    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);
    mbedtls_md5_update(&ctx, origin_product_ntp, strlen(origin_product_ntp)); // 更新有效数据部分的MD5值
    mbedtls_md5_finish(&ctx, output);
    mbedtls_md5_free(&ctx);
    for (int i = 0; i < 16; ++i) {
        sprintf(md5_hex_string + i * 2, "%02x", output[i]);
    }

    char req_body[256] = {'\0'};
    sprintf(req_body, "{\"productId\": \"%s\",\"deviceId\": \"%s\",\"curtime\": %s, \"checksum\": \"%s\"}",
            product_id, device_id, current_time, md5_hex_string);

    lisa_http_request_t req = {0};
    req.method = LISA_HTTP_POST;
    if (lisa_aiui_is_staging_mode_enable()) {
        req.url = AIUI_TOKEN_URL_STAGING;
    } else {
        req.url = AIUI_TOKEN_URL;
    }
    req.timeout = 10;
    req.on_data = http_aiui_token_on_data;
    req.body = req_body;
    req.body_len = strlen(req.body);
    req.headers = (uint8_t *)http_client_get_headers;

    lisa_http_t *http = lisa_http_init(&req);
    if (!http) {
        return NULL;
    }
    ret = lisa_http_perform(http);
    if (ret == LISA_HTTP_OK) {
        lisa_http_cleanup(http);
    } else {
        lisa_http_cleanup(http);
		lisa_mem_free(origin_product_ntp);
        return NULL;
    }
    LISA_LOGI(TAG, "reqbody: %s", req_body);
    lisa_mem_free(origin_product_ntp);

    char *aiui_token = NULL;
    ret = xQueueReceive(token_queue, &aiui_token, pdMS_TO_TICKS(5000));
    if (ret != pdTRUE) {
        LISA_LOGE(TAG, "get token timeout\n");
        return NULL;
    }
    LISA_LOGI(TAG, "got token: %s", aiui_token);
    return aiui_token;
}

static char *aiui_generate_url(void)
{
#define ONESHOT_PARAMS      "{\"scene\":\"main\", \"mcp\": \"true\"}"
#define CONTINUE_PARAMS     "{\"scene\":\"main\", \"type\": \"fullduplex\", \"mcp\": \"true\"}"
    char *params = (lisa_aiui_get_interactive_mode() == INTER_ONESHOT) ?
                                                        ONESHOT_PARAMS : CONTINUE_PARAMS;
	LISA_LOGI(TAG, "[%s]params: %s", __func__, params);
    char *params_base64 = aiui_base64_encode(params);
    if (strlen(params_base64) <= 0) {
        LISA_LOGE(TAG, "encode base64 error %s", params_base64);
    }

    const char *ws_base_path = "/v1/interaction?param=";

    char *ws_path = lisa_mem_calloc(1, strlen(ws_base_path) + strlen(params_base64) + 1);
    sprintf(ws_path, "%s%s", ws_base_path, params_base64);
    lisa_mem_free(params_base64);
    LISA_LOGI(TAG, "ws path: %s", ws_path);
    return ws_path;
}

__attribute__((weak)) int aiui_get_custom_token(char **token)
{
    return -1;
}

static int aiui_update_auto_header(lisa_aiui_t *aiui_handle, bool fresh_token)
{
    bool is_kv_pid = true;
    bool is_kv_sid = true;
    char *pid_ptr = NULL;
    char *sid_ptr = NULL;
	int r;

	r = lisa_kv_get_string(KV_KEY_USER_PID, &pid_ptr);
	if (r || pid_ptr == NULL) {
		LISA_LOGW(TAG, "get pid from kv failed, use default pid");
		pid_ptr = PRODUCT_ID;
		is_kv_pid = false;
	}

	r = lisa_kv_get_string(KV_KEY_USER_SID, &sid_ptr);
	if (r || sid_ptr == NULL) {
		LISA_LOGW(TAG, "get sid from kv failed, use default sid");
		sid_ptr = SECRET_ID;
		is_kv_sid = false;
	}

	LISA_LOGI(TAG, "pid: %s", pid_ptr);
	LISA_LOGI(TAG, "sid: %s", sid_ptr);

	char *token = NULL;
	bool is_kv_token = true;

	if (aiui_get_custom_token(&token) == 0) {
		is_kv_token = false;
	} else {
		if (!fresh_token) {
			r = lisa_kv_get_string(KV_KEY_TOKEN, &token);
			if (r || token == NULL) {
				LISA_LOGW(TAG, "get token from kv failed");
				fresh_token = true;
			}
		}

		if (fresh_token) {
			LISA_LOGI(TAG, "regenerate token");
			token = aiui_generate_token(pid_ptr, sid_ptr, id_buffer_str);
			is_kv_token = false;

			if (token) {
				r = lisa_kv_set_string(KV_KEY_TOKEN, token);
				if (r) {
					LISA_LOGE(TAG, "save token to kv failed");
				}
			}
		}
	}


	if (token == NULL) {
		LISA_LOGE(TAG, "token is null");
		goto exit;
	}

	aiui_handle->auth_token = lisa_mem_alloc(strlen(token) + 1);
	if (!aiui_handle->auth_token) {
		LISA_LOGE(TAG, "has no mem\n");
		goto exit;
	}
	strcpy(aiui_handle->auth_token, token);
	LISA_LOGI(TAG, "auth token: %s", aiui_handle->auth_token);

    aiui_handle->auth_header = lisa_mem_calloc(1, strlen(aiui_handle->auth_token) + strlen("\r\nAuthorization: Bearer ") + 1);
    if (!aiui_handle->auth_header) {
        LISA_LOGE(TAG, "has no mem\n");
        goto exit;
    }
    sprintf(aiui_handle->auth_header, "\r\nAuthorization: Bearer %s", aiui_handle->auth_token);
    LISA_LOGI(TAG, "auth header: %s", aiui_handle->auth_header);

exit:
    if (is_kv_pid) {
        lisa_kv_free(pid_ptr);
    }
    if (is_kv_sid) {
        lisa_kv_free(sid_ptr);
    }

    if (is_kv_token) {
        lisa_kv_free(token);
    } else {
		lisa_mem_free(token);
	}

    return 0;
}

void lisa_aiui_update_product_id(const char *pid)
{

}

void lisa_aiui_update_secret_id(const char *sid)
{
}

void lisa_aiui_clear_token(void)
{
}

lisa_aiui_config_t *lisa_aiui_get_config()
{
	if(s_lisa_aiui == NULL){
		return NULL;
	}
	
	return s_lisa_aiui->config;
}

lisa_err_t lisa_aiui_connect(lisa_aiui_t *const handle, bool update_token)
{
    if (lisa_aiui_is_staging_mode_enable()) {
        LISA_LOGI(TAG, "Using staging path mode");
    } else {
        LISA_LOGI(TAG, "Using production path mode");
    }
	// 生成url
    if (update_token) {
        lisa_aiui_clear_token();
    }

	LISA_LOGI(TAG, "is need refresh token: %d", update_token);
    aiui_update_auto_header(handle, update_token);

	char *url = aiui_generate_url();

	LISA_LOGI(TAG, "websocket connect url %s", url);
	lisa_ws_request_t ws_req;
	ws_req.timeout = AIUI_TIMEOUT;
	ws_req.on_data = get_ws_data_cb;
	ws_req.on_event = get_ws_event_cb;
	ws_req.user = NULL;
    if (lisa_aiui_is_staging_mode_enable()) {
        ws_req.host = AIUI_HOST_STAGING;
    } else {
        ws_req.host = AIUI_HOST;
    }
	ws_req.scheme = AIUI_SCHEME;
	ws_req.port = AIUI_PORT;
	ws_req.path = url;
    ws_req.extra_header = handle->auth_header;
	lisa_ws_t *lisa_ws_ins = lisa_ws_init(&ws_req);
	s_lisa_aiui->aiui_ws->ws_client = lisa_ws_ins;
	lisa_mem_free(url);

	if (lisa_ws_connect(lisa_ws_ins)) {
		LISA_LOGE(TAG, "lisa evs websocket connect error");
		return LISA_FAIL;
	}
	return LISA_OK;
}

/**
 * @brief 发送音频数据
 *
 * @param handle            sdk句柄
 * @param audio             音频数据
 * @param len               音频长度
 * @return lisa_err_t
 */
lisa_err_t lisa_aiui_send_audio(const lisa_aiui_t *handle, const void *audio, int len)
{
	if (handle) {
		int ret = lisa_ws_send_binary(handle->aiui_ws->ws_client, audio, len);
		if (ret == LISA_WS_OK) {
			return LISA_OK;
		}
	}
	return LISA_FAIL;
}

/**
 * @brief 发送文本数据
 *
 * @param handle            sdk句柄
 * @param txt               文本数据
 * @return lisa_err_t
 */
lisa_err_t lisa_aiui_send_txt(const lisa_aiui_t *handle, const char *const txt)
{
	if (handle) {
		int ret = lisa_ws_send_text(handle->aiui_ws->ws_client, txt);
		if (ret == LISA_WS_OK) {
			return LISA_OK;
		}
	}
	return LISA_FAIL;
}

lisa_err_t lisa_aiui_tts(const lisa_aiui_t *handle, const char *const txt)
{
#define TTS_FORMAT    "{\"action\":\"start\",\"params\":{\"features\":[\"tts\"],\"data_type\":\"text\"}}"
	if (handle) {
		int ret = lisa_ws_send_text(handle->aiui_ws->ws_client, TTS_FORMAT);
		if (ret == 0) {
			ret = lisa_ws_send_tts(handle->aiui_ws->ws_client, txt);
		}
		if (ret == 0) {
			return LISA_OK;
		}
	}
	return LISA_FAIL;
}

lisa_err_t lisa_aiui_start_send(const lisa_aiui_t *handle, const char *speaker)
{

#ifdef SEN_AUDIO_BY_ICO
#define ONESHOT_START_FORMAT    "{\"action\":\"start\",\"params\":{\"features\":[\"nlu\",\"tts\"],\"data_type\":\"audio\",\"aue\":\"ico\",\"nlu_properties\":{\"abilities\":[{\"name\":\"alarm\",\"intents\":[\"create\",\"cancel\"]}],\"sn\":\"%s\", \"lat\":\"%s\",\"lng\":\"%s\",\"custom\":{\"speaker\": \"%s\",\"userID\":\"%s\"}}}}"
#define CONTINUE_START_FORMAT   "{\"action\":\"start\",\"params\":{\"fullduplex_timeout\":\"30\",\"fullduplex\": \"1\",\"features\":[\"nlu\",\"tts\"],\"data_type\":\"audio\",\"aue\":\"ico\",\"nlu_properties\":{\"sn\":\"%s\",\"lat\":\"%s\",\"lng\":\"%s\",\"custom\":{\"rid\":\"123\", \"speaker\": \"%s\",\"userID\":\"%s\"}}}}"
#else
#define ONESHOT_START_FORMAT    "{\"action\":\"start\",\"params\":{\"features\":[\"nlu\",\"tts\"],\"data_type\":\"audio\",\"aue\":\"raw\",\"nlu_properties\":{\"abilities\":[{\"name\":\"alarm\",\"intents\":[\"create\",\"cancel\"]}],\"sn\":\"%s\", \"userID\":\"%s\",\"lat\":\"%s\",\"lng\":\"%s\"}}}"
#define CONTINUE_START_FORMAT   "{\"action\":\"start\",\"params\":{\"fullduplex\": \"1\",\"features\":[\"nlu\",\"tts\"],\"data_type\":\"audio\",\"aue\":\"raw\",\"nlu_properties\":{\"abilities\":[{\"name\":\"alarm\",\"intents\":[\"create\",\"cancel\"]}],\"sn\":\"%s\", \"userID\":\"%s\",\"lat\":\"%s\",\"lng\":\"%s\"}}}"
#endif

#define START_FORMAT_NO_VAD "{\"action\":\"start\",\"params\":{\"asr_properties\":{\"evad\":\"0\",\"svad\":\"0\",},\"features\":[\"nlu\",\"tts\"],\"data_type\":\"audio\",\"aue\":\"ico\",\"nlu_properties\":{\"abilities\":[{\"name\":\"alarm\",\"intents\":[\"create\",\"cancel\"]}],\"sn\":\"%s\",\"lat\":\"%s\",\"lng\":\"%s\",\"custom\":{\"userID\":\"%s\"}}}}"

    char *userid = NULL;
    char *lng = NULL;
    char *lat = NULL;

    if (handle == NULL) {
        return LISA_FAIL;
    }

    userid = "123456";
    lng = "22.53629851362625";
    lat = "113.94733033692522";

    int mode = lisa_aiui_get_interactive_mode();
    char *fmt = NULL;
    if (mode == INTER_ONESHOT) {
        fmt = ONESHOT_START_FORMAT;
    } else if (mode == INTER_CONTINUE) {
        fmt = CONTINUE_START_FORMAT;
    } else if (mode == INTER_BUTTON) {
        fmt = START_FORMAT_NO_VAD;
    }

    int fmt_len = strlen(fmt) + strlen(id_buffer_str) + 1 + strlen(userid) + strlen(lat) + strlen(lng) + strlen(speaker);

    char *start_format = lisa_mem_alloc(fmt_len);
    if (start_format == NULL) {
        LISA_LOGE(TAG, "lisa_aiui_start_send, has no mem\n");
        return LISA_FAIL;
    }

    snprintf(start_format, fmt_len, fmt, id_buffer_str, lat, lng, speaker, userid);
    lisa_ws_start(handle->aiui_ws->ws_client);
    LISA_LOGI(TAG, "aiui start msg: %s", start_format);
    int ret = lisa_ws_send_text(handle->aiui_ws->ws_client, start_format);
    lisa_mem_free(start_format);

    return ret;
}

lisa_err_t lisa_aiui_start_send_record(const lisa_aiui_t *handle)
{
	if (handle) {
		lisa_ws_start(handle->aiui_ws->ws_client);
	}
	return LISA_OK;
}

/**
 * @brief 发送END，停止识别
 *
 * @param handle            sdk句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_aiui_stop_send(const lisa_aiui_t *handle)
{
	lisa_ws_stop(handle->aiui_ws->ws_client);
    return LISA_OK;
}

/**
 * @brief 发送cancel，停止本次的识别,启动下一次识别
 *
 * @param handle            sdk句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_aiui_cancel_send(const lisa_aiui_t *handle)
{
	if (handle) {
		const char *cancel_tag = "{\"action\":\"cancel\"}";
		int ret = lisa_ws_send_text(handle->aiui_ws->ws_client, cancel_tag);
		// lisa_ws_stop(handle->aiui_ws->ws_client);
		if (ret == 0) {
			return LISA_OK;
		}
	}
    return LISA_OK;
}

lisa_err_t lisa_aiui_end_frame_send(const lisa_aiui_t *handle)
{
    if (handle) {
        const char *cancel_tag = "{\"action\":\"end\"}";
        int ret = lisa_ws_send_text(handle->aiui_ws->ws_client, cancel_tag);
        // lisa_ws_stop(handle->aiui_ws->ws_client);
        if (ret == 0) {
            return LISA_OK;
        }
    }
}

lisa_err_t lisa_aiui_disconnect(lisa_aiui_t *handle)
{
	if (handle) {
		if (handle->auth_token) {
			lisa_mem_free(handle->auth_token);
        	handle->auth_token = NULL;
		}
		if (handle->auth_header) {
			lisa_mem_free(handle->auth_header);
			handle->auth_header = NULL;
		}
		if (handle->aiui_ws->ws_client) {
			lisa_ws_cleanup(handle->aiui_ws->ws_client);
			handle->aiui_ws->ws_client = NULL;
		}
        LISA_LOGI(TAG, "disconnect done 1");
        return LISA_OK;
    }
    return LISA_FAIL;
}

lisa_err_t lisa_aiui_destroy(lisa_aiui_t *handle)
{
	if (handle) {
		lisa_ws_cleanup(handle->aiui_ws->ws_client);
		lisa_mem_free(handle->aiui_ws->ws_client);
		handle->aiui_ws->ws_client = NULL;
		lisa_mem_free(handle->aiui_ws);
        lisa_mem_free(handle->auth_token);
        lisa_mem_free(handle->auth_header);
        handle->auth_token = NULL;
        handle->auth_header = NULL;
        lisa_mem_free(handle);
	}

	return LISA_OK;
}

#define HTTP_ACTIVE_SERVER "https://adf.xfyun.cn/kuwo/active"
#define HTTP_ACTIVE_HEADER "Content-Type: application/x-www-form-urlencoded"
#define HTTP_BODY_FORMAT "appId=%s&token=%s&timestamp=%s&serialNumber=%s"
static void* _get_heads(void) { return HTTP_ACTIVE_HEADER; }

static void _http_on_data(lisa_http_data_t *data) {
	cJSON *root = cJSON_Parse(data->buf);
	if (root) {
		cJSON *param = cJSON_GetObjectItem(root, "code");
		if (param && param->valueint == 200) {
			LISA_LOGI(TAG, "active success!!!!");
			// evs_pref_put_int(MUSIC_ACTIVATING, 1);
		}
		cJSON_Delete(root);
	}
}

static void _active_func()
{
	lisa_aiui_config_t *cfg = lisa_aiui_get_config();
	lisa_http_request_t req;
	memset(&req, 0, sizeof(lisa_http_request_t));
	req.method = LISA_HTTP_POST;
	req.url = HTTP_ACTIVE_SERVER;
	req.timeout = 5;
	req.on_data = _http_on_data;

	char current_time[12];
	struct timeval tv;
	gettimeofday(&tv, NULL);
	sprintf(current_time, "%ld", (long)tv.tv_sec);

	int base_str_len = strlen(cfg->appid) + strlen(cfg->api_key) + strlen(current_time) + 1;
	char *origin_str = (char *)lisa_mem_alloc(base_str_len);
	sprintf(origin_str, "%s%s%s", cfg->appid, cfg->api_key, current_time);

	char hex_output[33];
	char md5_hash_origin[16];
	mbedtls_md5_context md5_ctx_origin;
	mbedtls_md5_init(&md5_ctx_origin);
	mbedtls_md5_starts(&md5_ctx_origin);
	mbedtls_md5_update(&md5_ctx_origin, origin_str, base_str_len - 1);
	mbedtls_md5_finish(&md5_ctx_origin, md5_hash_origin);
	mbedtls_md5_free(&md5_ctx_origin);
	for (int i = 0; i < 16; ++i) {
		sprintf(hex_output + i * 2, "%02x", md5_hash_origin[i]);
	}
	lisa_mem_free(origin_str);
	int body_len = strlen(HTTP_BODY_FORMAT) + strlen(cfg->appid) + strlen(current_time) +
				   strlen(hex_output) + strlen(current_time) + strlen(id_buffer_str);
	char *body = lisa_mem_alloc(body_len);
	sprintf(body, HTTP_BODY_FORMAT, cfg->appid, hex_output, current_time, id_buffer_str);
	req.body = body;
	req.body_len = strlen(body);
	req.user = NULL;
	req.headers = (uint8_t *)_get_heads;

	lisa_http_t *http = lisa_http_init(&req);
	lisa_mem_free(body);
	if (!http) {
		return;
	}
	if (lisa_http_perform(http) == LISA_HTTP_OK) {
		lisa_http_cleanup(http);
	} else {
		lisa_http_cleanup(http);
	}
}

lisa_err_t lisa_aiui_active()
{
	// int act = 0;
	// evs_pref_get_int(MUSIC_ACTIVATING, &act);
	// LISA_LOGD(TAG, "music active state %d\n", act);
	// if (act == 0) {
		_active_func();
	// }
	return LISA_OK;
}

lisa_err_t lisa_aiui_not_active()
{
	return LISA_OK;
}

#define HTTP_REQUEST_URL_SERVER "http://adf.xfyun.cn/kuwo/tranklink"
#define HTTP_REQUEST_URL_HEADER "Content-Type: application/x-www-form-urlencoded"
#define HTTP_REQ_URL_BODY_FORMAT "appId=%s&token=%s&timestamp=%s&serialNumber=%s&itemid=%s&format=128kmp3"

static void* _req_url_get_heads(void) { return HTTP_REQUEST_URL_HEADER; }
static void _req_url_http_on_data(lisa_http_data_t *data) {
	LISA_LOGI(TAG, "_req_url_http_on_data, data: %s", data->buf);
	cJSON *root = cJSON_Parse(data->buf);
	if (root) {
		cJSON *param = cJSON_GetObjectItem(root, "data");
		if (param && cJSON_GetArraySize(param) > 0) {
			cJSON *item = cJSON_GetArrayItem(param, 0);
			if (item) {
				cJSON *url = cJSON_GetObjectItem(item, "audiopath");
				if (url && url->valuestring) {
					LISA_LOGI(TAG, "audio url : %s ", url->valuestring);
					strcpy(data->user, url->valuestring);
				}
			}
		}
		cJSON_Delete(root);
	}
}

void ls_req_url(const char *item_id, char *url)
{
	lisa_aiui_config_t *cfg = lisa_aiui_get_config();
	lisa_http_request_t req;
	memset(&req, 0, sizeof(lisa_http_request_t));
	req.method = LISA_HTTP_POST;
	req.url = HTTP_REQUEST_URL_SERVER;
	req.timeout = 2;
	req.on_data = _req_url_http_on_data;

	char current_time[12];
	struct timeval tv;
	gettimeofday(&tv, NULL);
	sprintf(current_time, "%ld", (long)tv.tv_sec);

	int base_str_len = strlen(cfg->appid) + strlen(cfg->api_key) + strlen(current_time) + 1;
	char *origin_str = (char *)lisa_mem_alloc(base_str_len);
	sprintf(origin_str, "%s%s%s", cfg->appid, cfg->api_key, current_time);

	char hex_output[33];
	char md5_hash_origin[16];
	mbedtls_md5_context md5_ctx_origin;
	mbedtls_md5_init(&md5_ctx_origin);
	mbedtls_md5_starts(&md5_ctx_origin);
	mbedtls_md5_update(&md5_ctx_origin, origin_str, base_str_len - 1);
	mbedtls_md5_finish(&md5_ctx_origin, md5_hash_origin);
	mbedtls_md5_free(&md5_ctx_origin);
	for (int i = 0; i < 16; ++i) {
		sprintf(hex_output + i * 2, "%02x", md5_hash_origin[i]);
	}
	lisa_mem_free(origin_str);
	int body_len = strlen(HTTP_REQ_URL_BODY_FORMAT) + strlen(cfg->appid) + strlen(current_time) +
				   strlen(hex_output) + strlen(current_time) + strlen(item_id) + strlen(get_device_id_str());
	char *body = lisa_mem_alloc(body_len);
	sprintf(body, HTTP_REQ_URL_BODY_FORMAT, cfg->appid, hex_output, current_time, get_device_id_str(), item_id);
	req.body = body;
	req.body_len = strlen(body);
	req.user = url;
	req.headers = (uint8_t *)_req_url_get_heads;

	lisa_http_t *http = lisa_http_init(&req);
	lisa_mem_free(body);
	if (!http) {
		return;
	}
	if (lisa_http_perform(http) == LISA_HTTP_OK) {
		lisa_http_cleanup(http);
	} else {
		lisa_http_cleanup(http);
	}
}

int lisa_aiui_img_recognition(lisa_aiui_t *hd, const void *img_data, int img_len)
{
	/* send start frame */
	int err;
	extern int started_wait(int timeout);
	extern void started_reset(void);

	const char *fmt = "{\"action\":\"start\",\"params\":{\"data_type\":\"text\",\"nlu_properties\":{\"llmParams\":{\"model\":\"NULL\"},\"custom\":{\"sceneId\": 217,\"image\":\"%s\"}}}}";

	uint8_t *base64_data = aiui_base64_encodev2(img_data, img_len);
	if (base64_data == NULL) {
		LISA_LOGE(TAG, "lisa_aiui_img_recognition failed, base64_data is null");
		return LISA_FAIL;
	}

	int size = strlen(fmt) + strlen(base64_data) + 1;
	char *fmt_data = (char *)lisa_mem_alloc(size);
	if (fmt_data == NULL) {
		LISA_LOGE(TAG, "lisa_aiui_img_recognition failed, fmt_data is null");
		return LISA_FAIL;
	}
	int l = snprintf(fmt_data, size, fmt, base64_data);
	fmt_data[l] = '\0';

	LISA_LOGI(TAG, "lisa_aiui_img_recognition fmt_data: %s", fmt_data);

	started_reset();
	err = lisa_ws_send_text(hd->aiui_ws->ws_client, fmt_data);
	lisa_mem_free(fmt_data);
	lisa_mem_free(base64_data);
	if (err) {
		LISA_LOGE(TAG, "lisa_aiui_img_recognition failed, err:%d", err);
		return err;
	}
	started_wait(10 * 1000);
	const uint8_t prompt[] = "这个图片里面有什么东西";
	err = lisa_ws_send_bin(hd->aiui_ws->ws_client, prompt, sizeof(prompt));

	if (err) {
		LISA_LOGE(TAG, "aiui_picture_send_text failed, err:%d", err);
		return err;
	}

	return LISA_OK;
}
#include "cJSON.h"

int lisa_aiui_start_frame_send_txt(lisa_aiui_t *handle, const char *txt)
{
    return lisa_ws_send_text(handle->aiui_ws->ws_client, txt);
}

static uint32_t lisa_ui_rid_get(void)
{
    return (uint32_t)lisa_rand32();
}

int lisa_aiui_start_frame_send(lisa_aiui_t *handle, cJSON *root, uint32_t type)
{
    int err;

    if (root == NULL) {
        return -1;
    }

	cJSON *params = cJSON_GetObjectItem(root, "params");
	if (params == NULL) {
		return -1;
	}

	cJSON *nlu_properties = cJSON_GetObjectItem(params, "nlu_properties");
	if (nlu_properties == NULL) {
		return -1;
	}

	cJSON *custom = cJSON_GetObjectItem(nlu_properties, "custom");
	if (custom == NULL) {
		custom = cJSON_CreateObject();
		cJSON_AddItemToObject(nlu_properties, "custom", custom);
	}

	cJSON *rid = cJSON_GetObjectItem(custom, "rid");
	if (rid) {
		cJSON_DeleteItemFromObject(custom, "rid");
	}

	char rid_str[12] = {0};
	uint32_t rid_num = lisa_ui_rid_get();
	int l = snprintf(rid_str, 11, "%u", rid_num);
	rid_str[l] = '\0';
	cJSON_AddStringToObject(custom, "rid", rid_str);

    char *txt = cJSON_PrintUnformatted(root);
    if (txt == NULL) {
        return -1;
    }

	lisa_ui_rid_list_clear();

    err = lisa_aiui_rid_list_add(rid_num, (void *)type);
	if (err) {
		LISA_LOGE(TAG, "lisa_aiui_rid_list_add failed, rid: %u, err:%d", rid_num, err);
	}

    err = lisa_aiui_start_frame_send_txt(handle, txt);
	LISA_LOGI(TAG, "img start: %s", txt);
    cJSON_free(txt);

    if (err) {
		lisa_aiui_rid_list_remove(rid_num);
        return err;
	}

    return 0;
}

int lisa_aiui_start_frame_send_audio(lisa_aiui_t *handle)
{
    cJSON *root;
    int err;

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "action", cJSON_CreateString("start"));

    cJSON *params = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "params", params);
    cJSON_AddStringToObject(params, "fullduplex_timeout", "60");
    cJSON_AddStringToObject(params, "fullduplex", "1");

    cJSON *features = cJSON_CreateArray();
    cJSON_AddItemToArray(features, cJSON_CreateString("nlu"));
    cJSON_AddItemToArray(features, cJSON_CreateString("tts"));
    cJSON_AddItemToObject(params, "features", features);

    cJSON_AddStringToObject(params, "data_type", "audio");
    cJSON_AddStringToObject(params, "aue", "ico");

    cJSON *nlu_properties = cJSON_CreateObject();
    cJSON_AddItemToObject(params, "nlu_properties", nlu_properties);

	lisa_ws_start(handle->aiui_ws->ws_client);

	err = lisa_aiui_start_frame_send(handle, root, LISA_AIUI_FRAME_TYPE_AUDIO);
    cJSON_Delete(root);

    return err;
}

int lisa_aiui_chat_send(lisa_aiui_t *handle, const char *txt)
{
    cJSON *root;
    int err;

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "action", cJSON_CreateString("start"));
    cJSON *params = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "params", params);
    cJSON_AddItemToObject(params, "data_type", cJSON_CreateString("text"));
    cJSON *nlu_properties = cJSON_CreateObject();
    cJSON_AddItemToObject(params, "nlu_properties", nlu_properties);

    err = lisa_aiui_start_frame_send(handle, root, LISA_AIUI_FRAME_TYPE_TEXT);
    cJSON_Delete(root);

	lisa_ws_send_tts(handle->aiui_ws->ws_client, txt);

    return err;
}

int lisa_ui_tts_send(lisa_aiui_t *handle, const char *text)
{
    cJSON *root;
    int err;

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "action", cJSON_CreateString("start"));
    cJSON *params = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "params", params);
    cJSON_AddItemToObject(params, "data_type", cJSON_CreateString("text"));
	// cJSON *features = cJSON_CreateArray();
	// cJSON_AddItemToArray(features, cJSON_CreateString("tts"));
	// cJSON_AddItemToObject(params, "features", features);

    cJSON *nlu_properties = cJSON_CreateObject();
    cJSON_AddItemToObject(params, "nlu_properties", nlu_properties);

	lisa_aiui_start_frame_send(handle, root, LISA_AIUI_FRAME_TYPE_TTS);
	cJSON_Delete(root);

	lisa_ws_send_tts(handle->aiui_ws->ws_client, text);

	return 0;
}

int lisa_aiui_start_frame_send_base64img(lisa_aiui_t *handle, const char *image_base64)
{
    cJSON *root;
    int err;

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "action", cJSON_CreateString("start"));
    cJSON *params = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "params", params);
    cJSON_AddItemToObject(params, "data_type", cJSON_CreateString("text"));
    cJSON *nlu_properties = cJSON_CreateObject();
    cJSON_AddItemToObject(params, "nlu_properties", nlu_properties);
    cJSON *custom = cJSON_CreateObject();
    cJSON_AddStringToObject(custom, "image", image_base64);
    cJSON_AddItemToObject(nlu_properties, "custom", custom);

    err = lisa_aiui_start_frame_send(handle, root, LISA_AIUI_FRAME_TYPE_IMAGE);
    cJSON_Delete(root);

    return err;
}

int lisa_aiui_send_jpg_img(lisa_aiui_t *handle, const void *img_data, int img_len)
{
	int err;

    uint8_t *base64_data = aiui_base64_encodev2(img_data, img_len);
    if (base64_data == NULL) {
        LISA_LOGE(TAG, "lisa_aiui_img_recognition failed, base64_data is null");
        return LISA_FAIL;
    }

	err = lisa_aiui_start_frame_send_base64img(handle, base64_data);
	lisa_mem_free(base64_data);

	if (err) {
		return err;
	}

	const uint8_t prompt[] = "这个图片里面有什么东西";
	err = lisa_ws_send_bin(handle->aiui_ws->ws_client, prompt, sizeof(prompt));
	if (err) {
		return err;
	}

    return 0;
}
