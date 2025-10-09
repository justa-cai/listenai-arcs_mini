#include <stdio.h>
#include <string.h>
#include "lisa_evs_auth.h"
#include "lisa_mem.h"
#include "lisa_thread.h"
#include "lisa_typedef.h"
#include "lisa_log.h"
#include "lisa_http.h"
#include "utils/evs_utils.h"
#include "cJSON.h"


#define LISA_EVS_REQ_DEVICE_CODE_HEADS ("Content-Type:application/x-www-form-urlencoded;charset=utf-8")
#define LISA_EVS_REQ_TOKEN_HEADS ("Content-Type:application/json;charset=utf-8")

#define URL_GET_DEVICECODE ("https://auth.iflyos.cn/oauth/ivs/device_code")
#define URL_REFRESH_TOKEN ("https://api.iflyos.cn/thirdparty/general/quick_auth")
#define URL_ACCESS_TOKEN ("https://auth.iflyos.cn/oauth/ivs/token")
#define MAX_SIZE_TOKEN (256)
#define MAX_MSG_LEN (1024)
#define HTTP_TIME_OUT (10)
#define REQUEST_STACK_SIZE (4 * 1024)

#define TAG "lisa_auth"

typedef struct lisa_auth_context {
	const lisa_evs_auth_t *m_handle;
	char *m_device_code;
	char *m_user_code;
	char *m_refresh_token;
} lisa_auth_context_t;

static lisa_http_t *g_auth_http = NULL;
static lisa_thread_t *g_request_thread = NULL;

lisa_evs_auth_t *lisa_evs_auth_create(const lisa_evs_auth_cb_t *const auth_cb, const lisa_evs_config_t *const config)
{
	LISA_LOGD(TAG, "lisa evs auth create [START]");
	lisa_evs_auth_t *evs_auth = (lisa_evs_auth_t *)lisa_mem_calloc(1, sizeof(lisa_evs_auth_t));
	lisa_evs_config_t *evs_config = NULL;
	uint8_t client_id_len = 0;
	uint8_t device_id_len = 0;

	if (!evs_auth) {
		LISA_LOGE(TAG, "alloc lisa_evs_auth pointer error.");
		goto LISA_EVS_AUTH_EXIT_ERROR;
	}

	evs_auth->evs_auth_cb = auth_cb;

	// alloc config
	evs_config = (lisa_evs_config_t *)lisa_mem_calloc(1, sizeof(lisa_evs_config_t));
	if (!evs_config) {
		LISA_LOGE(TAG, "alloc config pointer error.");
		goto LISA_EVS_AUTH_EXIT_ERROR_CONFIG;
	}

	// client_id
	client_id_len = strlen(config->client_id);
	evs_config->client_id = (char *)lisa_mem_alloc(sizeof(char) * (client_id_len + 1));
	if (!evs_config->client_id) {
		LISA_LOGE(TAG, "alloc client_id pointer error.");
		goto LISA_EVS_AUTH_EXIT_ERROR_CLIENTID;
	}
	memcpy(evs_config->client_id, config->client_id, client_id_len);
	evs_config->client_id[client_id_len] = '\0';

	// device_id
	device_id_len = strlen(config->device_id);
	evs_config->device_id = (char *)lisa_mem_alloc(sizeof(char) * (device_id_len + 1));
	if (!evs_config->device_id) {
		LISA_LOGE(TAG, "alloc device_id pointer error.");
		goto LISA_EVS_AUTH_EXIT_ERROR_DEVICEID;
	}
	memcpy(evs_config->device_id, config->device_id, device_id_len);
	evs_config->device_id[device_id_len] = '\0';

	evs_auth->evs_config = evs_config;

	goto LISA_EVS_AUTH_EXIT_SUCCESS;

LISA_EVS_AUTH_EXIT_ERROR_DEVICEID:
	lisa_mem_free(evs_config->client_id);
LISA_EVS_AUTH_EXIT_ERROR_CLIENTID:
	lisa_mem_free(evs_config);
LISA_EVS_AUTH_EXIT_ERROR_CONFIG:
	lisa_mem_free(evs_auth);
LISA_EVS_AUTH_EXIT_ERROR:
	evs_auth = NULL;
LISA_EVS_AUTH_EXIT_SUCCESS:
	LISA_LOGD(TAG, "lisa evs auth create [END]");
	return evs_auth;
}

lisa_err_e lisa_evs_auth_destory(const lisa_evs_auth_t *const handle)
{
	LISA_LOGD(TAG, "lisa evs auth destory [START]");
	if (handle) {
		if (handle->evs_config) {
			if (handle->evs_config->client_id) {
				lisa_mem_free(handle->evs_config->client_id);
			}
			if (handle->evs_config->device_id) {
				lisa_mem_free(handle->evs_config->device_id);
			}
			lisa_mem_free((void *)handle->evs_config);
		}

		lisa_mem_free((void *)handle);
	}
	LISA_LOGD(TAG, "lisa evs auth destory [END]");
	return LISA_CODE_SUCCESS;
}

static void* _lisa_evs_request_device_code_heads(void) { return LISA_EVS_REQ_DEVICE_CODE_HEADS; }

static void* _lisa_evs_request_token_heads(void) { return LISA_EVS_REQ_TOKEN_HEADS; }

void get_devicecode_data_cb(lisa_http_data_t *data)
{
	lisa_evs_auth_t *handle = (lisa_evs_auth_t *)data->user;
	char *msg = NULL;
	cJSON *info = NULL;

	if (handle == NULL || handle->evs_auth_cb == NULL) {
		LISA_LOGE(TAG, "parse devicecode callback is null");
		return;
	}

	msg = (char *)data->buf;
	info = cJSON_Parse(msg);
	if (info != NULL) {
		if (cJSON_HasObjectItem(info, "error")) {
			if (handle->evs_auth_cb->get_devicecode_failed != NULL) {
				cJSON *err_msg = cJSON_GetObjectItem(info, "error");
				handle->evs_auth_cb->get_devicecode_failed(err_msg->valuestring);
			}
		} else {
			cJSON *user_code_json = cJSON_GetObjectItem(info, "user_code");
			cJSON *device_code_json = cJSON_GetObjectItem(info, "device_code");
			lisa_evs_get_devicecode_response_t response;
			strcpy(response.device_code, device_code_json->valuestring);
			strcpy(response.user_code, user_code_json->valuestring);

			if (handle->evs_auth_cb->get_devicecode_success != NULL) {
				handle->evs_auth_cb->get_devicecode_success(&response);
			}
		}
		cJSON_Delete(info);
	} else {
		if (handle->evs_auth_cb->get_devicecode_failed != NULL) {
			handle->evs_auth_cb->get_devicecode_failed(msg);
		}
	}
}

void get_token_data_cb(lisa_http_data_t *data)
{
	lisa_evs_auth_t *handle = (lisa_evs_auth_t *)data->user;
	char *msg = NULL;
	cJSON *info = NULL;
	if (handle == NULL || handle->evs_auth_cb == NULL) {
		LISA_LOGE(TAG, "parseToken callback is null");
		return;
	}
	msg = (char *)data->buf;
	info = cJSON_Parse(msg);
	if (info != NULL) {
		if (cJSON_HasObjectItem(info, "error")) {
			if (handle->evs_auth_cb->get_token_failed != NULL) {
				cJSON *err_msg = cJSON_GetObjectItem(info, "error");
				handle->evs_auth_cb->get_token_failed(err_msg->valuestring);
			}
		} else {
			cJSON *token_type = cJSON_GetObjectItem(info, "token_type");
			cJSON *refresh_token = cJSON_GetObjectItem(info, "refresh_token");
			cJSON *expires_in = cJSON_GetObjectItem(info, "expires_in");
			cJSON *created_at = cJSON_GetObjectItem(info, "created_at");
			cJSON *access_token = cJSON_GetObjectItem(info, "access_token");
			lisa_evs_get_token_response_t response;
			strcpy(response.m_token_type, token_type->valuestring);
			strcpy(response.m_refresh_token, refresh_token->valuestring);
			strcpy(response.m_access_token, access_token->valuestring);
			response.m_created_time = created_at->valuedouble;
			response.m_expires_in = expires_in->valuedouble;
			if (handle->evs_auth_cb->get_token_success != NULL) {
				handle->evs_auth_cb->get_token_success(&response);
			}
		}
		cJSON_Delete(info);
	} else {
		if (handle->evs_auth_cb->get_token_failed != NULL) {
			handle->evs_auth_cb->get_token_failed(msg);
		}
	}
}

static void _auth_request_code_thread(void *param)
{
	lisa_auth_context_t *ins = (lisa_auth_context_t*)param;
	int ret = 0;
	lisa_http_request_t *http_param = NULL;
	cJSON *scope_json = NULL;
	cJSON *device_json = NULL;
	char *scope_str = NULL;
	char out_str[128] = {0};
	char request_body[256] = {0};
	lisa_thread_t *request_thread = NULL;

	if (ins == NULL || ins->m_handle == NULL || ins->m_handle->evs_config == NULL ||
		(ins->m_handle->evs_config->client_id == NULL || ins->m_handle->evs_config->client_id[0] == '\0') ||
		(ins->m_handle->evs_config->device_id == NULL || ins->m_handle->evs_config->device_id[0] == '\0')) {
		ret = -1;
		goto ERR;
	}

	if (g_auth_http) {
		LISA_LOGE(TAG, "error:  http has exist");
		ret = -1;
		goto ERR;
	}
	http_param = (lisa_http_request_t *)lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	if (http_param == NULL) {
		LISA_LOGE(TAG, "malloc http params error");
		ret = -1;
		goto ERR;
	}

	scope_json = cJSON_CreateObject();
	device_json = cJSON_CreateObject();
	cJSON_AddStringToObject(device_json, "device_id", ins->m_handle->evs_config->device_id);
	cJSON_AddItemToObject(scope_json, "user_ivs_all", device_json);
	scope_str = cJSON_PrintUnformatted(scope_json);
	lisa_evs_urlencode(scope_str, strlen(scope_str), out_str, sizeof(out_str));
	cJSON_Delete(scope_json);
	cJSON_free(scope_str);
	sprintf(request_body, "client_id=%s&scope=user_ivs_all user_device_text_in&scope_data=%s", ins->m_handle->evs_config->client_id, out_str);
	// LISA_LOGI(TAG, "request_body: %s", request_body);

	http_param->method = LISA_HTTP_POST;
	http_param->url = URL_GET_DEVICECODE;
	http_param->headers = (void *)_lisa_evs_request_device_code_heads;
	http_param->body = request_body;
	http_param->body_len = strlen(request_body);
	http_param->timeout = HTTP_TIME_OUT;
	http_param->on_data = get_devicecode_data_cb;

	http_param->user = (void *)ins->m_handle;

	g_auth_http = lisa_http_init(http_param);
	if (!g_auth_http) {
		LISA_LOGE(TAG, " http  error");
		ret = -1;
		goto ERR;
	}

	if ((ret = lisa_http_perform(g_auth_http)) != 0) {
		LISA_LOGE(TAG, "http get request info err..");
		ret = -1;
		goto ERR;
	}

ERR:
	if (g_auth_http) {
		lisa_http_cleanup(g_auth_http);
		g_auth_http = NULL;
	}
	if (http_param) {
		lisa_mem_free(http_param);
	}
	if (ins) {
		if (ret == -1) {
			if (ins->m_handle->evs_auth_cb->get_token_failed != NULL) {
				ins->m_handle->evs_auth_cb->get_token_failed("请求失败");
			}
		}
		if (ins->m_user_code) {lisa_mem_free(ins->m_user_code);}
		if (ins->m_device_code) {lisa_mem_free(ins->m_device_code);}
		if (ins->m_refresh_token) {lisa_mem_free(ins->m_refresh_token);}
		lisa_mem_free(ins);
	}

	if (g_request_thread) {
		request_thread = g_request_thread;
		g_request_thread = NULL;
		lisa_thread_delete(request_thread);
	}
    return;
}

lisa_err_e lisa_evs_auth_request_device_code(const lisa_evs_auth_t *const handle)
{
	if (handle == NULL || handle->evs_config == NULL || 
		(handle->evs_config->client_id == NULL || handle->evs_config->client_id[0] == '\0') ||
		(handle->evs_config->device_id == NULL || handle->evs_config->device_id[0] == '\0')) {
		return -1;
	}

	if (g_request_thread) {
		LISA_LOGE(TAG, "lisa auth request running");
		return -1;
	}
	lisa_auth_context_t *auth_context = (lisa_auth_context_t *)lisa_mem_calloc(1, sizeof(lisa_auth_context_t));
	if (!auth_context) {
		LISA_LOGE(TAG, "lisa auth context malloc failed");
		return -1;
	}
	
	auth_context->m_handle = handle;

	lisa_thread_attr_t thread_attr;
	thread_attr.name = "lisa_evs_auth_thread";
	thread_attr.stack_size = REQUEST_STACK_SIZE;
	thread_attr.priority = LISA_OS_PRIORITY_NORMAL; // OS_PRIORITY_NORMAL
	g_request_thread = lisa_thread_create(&thread_attr, _auth_request_code_thread, (void *)auth_context);
	if (g_request_thread == NULL) {
		if (auth_context) {lisa_mem_free(auth_context);}
		LISA_LOGE(TAG, "lisa auth thread_create failed");
		return -1;
	}
	return 0;
}

static void _auth_request_refreshtoken_thread(void *param)
{
	lisa_auth_context_t *ins = (lisa_auth_context_t*)param;
	int ret = 0;
	lisa_http_request_t *http_param = NULL;
	cJSON *msg_json = NULL;
	char *request_body = NULL;
	lisa_thread_t *request_thread = NULL;

	if (ins == NULL || ins->m_handle == NULL || ins->m_handle->evs_config == NULL) {
		LISA_LOGE(TAG, "handle or evs config is null");
		ret = -1;
		goto ERR;
	}
	if ((ins->m_handle->evs_config->client_id == NULL || ins->m_handle->evs_config->client_id[0] == '\0') ||
		(ins->m_handle->evs_config->device_id == NULL || ins->m_handle->evs_config->device_id[0] == '\0')) {
		LISA_LOGE(TAG, "client id or device id is null or empty");
		ret = -1;
		goto ERR;
	}
	if ((ins->m_user_code == NULL || ins->m_user_code[0] == '\0') ||
		(ins->m_device_code == NULL || ins->m_device_code[0] == '\0')) {
		LISA_LOGE(TAG, "usr code or device code is null or empty");
		ret = -1;
		goto ERR;
	}

	if (g_auth_http) {
		LISA_LOGE(TAG, "error:  http has exist");
		ret = -1;
		goto ERR;
	}
	http_param = (lisa_http_request_t *)lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	if (http_param == NULL) {
		LISA_LOGE(TAG, "malloc http params error");
		ret = -1;
		goto ERR;
	}
	msg_json = cJSON_CreateObject();
	cJSON_AddStringToObject(msg_json, "client_id", ins->m_handle->evs_config->client_id);
	cJSON_AddStringToObject(msg_json, "thirdparty_id", ins->m_handle->evs_config->device_id);
	cJSON_AddStringToObject(msg_json, "grant_type", "urn:ietf:params:oauth:grant-type:device_code");
	cJSON_AddStringToObject(msg_json, "user_code", ins->m_user_code);
	cJSON_AddStringToObject(msg_json, "device_code", ins->m_device_code);
	request_body = cJSON_PrintUnformatted(msg_json);
	cJSON_Delete(msg_json);
	http_param->method = LISA_HTTP_POST;
	http_param->url = URL_REFRESH_TOKEN;
	http_param->headers = (void *)_lisa_evs_request_token_heads;
	http_param->body = request_body;
	http_param->body_len = strlen(request_body);
	http_param->timeout = HTTP_TIME_OUT;
	http_param->on_data = (void *)get_token_data_cb;

	http_param->user = (void *)ins->m_handle;

	g_auth_http = lisa_http_init(http_param);
	if (!g_auth_http) {
		LISA_LOGE(TAG, " http  error");
		ret = -1;
		goto ERR;
	}

	if ((ret = lisa_http_perform(g_auth_http)) != 0) {
		LISA_LOGE(TAG, "http get request info err..");
		ret = -1;
		goto ERR;
	}

ERR:
	if (g_auth_http) {
		lisa_http_cleanup(g_auth_http);
		g_auth_http = NULL;
	}
	if (request_body) {
		cJSON_free(request_body);
	}
	if (http_param) {
		lisa_mem_free(http_param);
	}
	if (ins) {
		if (ret == -1) {
			if (ins->m_handle->evs_auth_cb->get_token_failed != NULL) {
				ins->m_handle->evs_auth_cb->get_token_failed("请求失败");
			}
		}
		if (ins->m_user_code) {lisa_mem_free(ins->m_user_code);}
		if (ins->m_device_code) {lisa_mem_free(ins->m_device_code);}
		if (ins->m_refresh_token) {lisa_mem_free(ins->m_refresh_token);}
		lisa_mem_free(ins);
	}
	if (g_request_thread) {
		request_thread = g_request_thread;
		g_request_thread = NULL;
		lisa_thread_delete(request_thread);
	}
    return;
}

lisa_err_e lisa_evs_auth_request_refreshtoken(
		const lisa_evs_auth_t *const handle, const uint8_t *const user_code, const uint8_t *const device_code)
{
	if (handle == NULL || handle->evs_config == NULL) {
		LISA_LOGE(TAG, "handle or evs config is null");
		return -1;
	}
	if ((handle->evs_config->client_id == NULL || handle->evs_config->client_id[0] == '\0') ||
		(handle->evs_config->device_id == NULL || handle->evs_config->device_id[0] == '\0')) {
		LISA_LOGE(TAG, "client id or device id is null or empty");
		return -1;
	}
	if ((user_code == NULL || user_code[0] == '\0') ||
		(device_code == NULL || device_code[0] == '\0')) {
		LISA_LOGE(TAG, "usr code or device code is null or empty");
		return -1;
	}

	if (g_request_thread) {
		LISA_LOGE(TAG, "lisa auth request running");
		return -1;
	}
	lisa_auth_context_t *auth_context = (lisa_auth_context_t *)lisa_mem_calloc(1, sizeof(lisa_auth_context_t));
	if (!auth_context) {
		LISA_LOGE(TAG, "lisa auth context malloc failed");
		return -1;
	}

	auth_context->m_handle = handle;
	int length = strlen(user_code);
	auth_context->m_user_code = (char *)lisa_mem_calloc(1, sizeof(char) * (length + 1));
	memcpy(auth_context->m_user_code, user_code, length);
	auth_context->m_user_code[length] = '\0';

	length = strlen(device_code);
	auth_context->m_device_code = (char *)lisa_mem_calloc(1, sizeof(char) * (length + 1));
	memcpy(auth_context->m_device_code, device_code, length);
	auth_context->m_device_code[length] = '\0';

	lisa_thread_attr_t thread_attr;
	thread_attr.name = "lisa_evs_auth_thread";
	thread_attr.stack_size = REQUEST_STACK_SIZE;
	thread_attr.priority = LISA_OS_PRIORITY_NORMAL; // OS_PRIORITY_NORMAL
	g_request_thread = lisa_thread_create(&thread_attr, _auth_request_refreshtoken_thread, (void *)auth_context);
	if (g_request_thread == NULL) {
		if (auth_context) {
			if (auth_context->m_user_code) {lisa_mem_free(auth_context->m_user_code);}
			if (auth_context->m_device_code) {lisa_mem_free(auth_context->m_device_code);}
			lisa_mem_free(auth_context);
		}
		LISA_LOGE(TAG, "lisa auth thread_create failed");
		return -1;
	}
	return 0;
}

static void _auth_request_accesstoken_thread(void *param)
{
	lisa_auth_context_t *ins = (lisa_auth_context_t*)param;
	int ret = 0;
	lisa_http_request_t *http_param = NULL;
	cJSON *msg_json = NULL;
	char *request_body = NULL;
	lisa_thread_t *request_thread = NULL;

	if (ins->m_handle == NULL || (ins->m_refresh_token == NULL || ins->m_refresh_token[0] == '\0')) {
		ret = -1;
		goto ERR;
	}

	if (g_auth_http) {
		LISA_LOGE(TAG, "error:  http has exist");
		ret = -1;
		goto ERR;
	}
	http_param = (lisa_http_request_t *)lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	if (http_param == NULL) {
		LISA_LOGE(TAG, "malloc http params error");
		ret = -1;
		goto ERR;
	}
	msg_json = cJSON_CreateObject();
	cJSON_AddStringToObject(msg_json, "grant_type", "refresh_token");
	cJSON_AddStringToObject(msg_json, "refresh_token", ins->m_refresh_token);
	request_body = cJSON_PrintUnformatted(msg_json);
	cJSON_Delete(msg_json);
	http_param->method = LISA_HTTP_POST;
	http_param->url = URL_ACCESS_TOKEN;
	http_param->headers = (void *)_lisa_evs_request_token_heads;
	http_param->body = request_body;
	http_param->body_len = strlen(request_body);
	http_param->timeout = HTTP_TIME_OUT;
	http_param->on_data = (void *)get_token_data_cb;

	http_param->user = (void *)ins->m_handle;

	g_auth_http = lisa_http_init(http_param);
	if (!g_auth_http) {
		LISA_LOGE(TAG, " http  error");
		ret = -1;
		goto ERR;
	}

	if ((ret = lisa_http_perform(g_auth_http)) != 0) {
		LISA_LOGE(TAG, "http get request info err..");
		ret = -1;
		goto ERR;
	}

ERR:
	if (g_auth_http) {
		lisa_http_cleanup(g_auth_http);
		g_auth_http = NULL;
	}
	if (request_body) {
		cJSON_free(request_body);
	}
	if (http_param) {
		lisa_mem_free(http_param);
	}
	if (ins) {
		if (ret == -1) {
			if (ins->m_handle->evs_auth_cb->get_token_failed != NULL) {
				ins->m_handle->evs_auth_cb->get_token_failed("请求失败");
			}
		}
		if (ins->m_user_code) {lisa_mem_free(ins->m_user_code);}
		if (ins->m_device_code) {lisa_mem_free(ins->m_device_code);}
		if (ins->m_refresh_token) {lisa_mem_free(ins->m_refresh_token);}
		lisa_mem_free(ins);
	}
	if (g_request_thread) {
		request_thread = g_request_thread;
		g_request_thread = NULL;
		lisa_thread_delete(request_thread);
	}
    return;
}

lisa_err_e lisa_evs_auth_request_accesstoken(
		const lisa_evs_auth_t *const handle, const uint8_t *const refresh_token)
{
	if (handle == NULL || (refresh_token == NULL || refresh_token[0] == '\0')) {
		return -1;
	}
	if (g_request_thread) {
		LISA_LOGE(TAG, "lisa auth request running");
		return -1;
	}
	lisa_auth_context_t *auth_context = (lisa_auth_context_t *)lisa_mem_calloc(1, sizeof(lisa_auth_context_t));
	auth_context->m_handle = handle;
	int length = strlen(refresh_token);
	auth_context->m_refresh_token = (char *)lisa_mem_calloc(1, sizeof(char) * (length + 1));
	memcpy(auth_context->m_refresh_token, refresh_token, length);
	auth_context->m_refresh_token[length] = '\0';

	lisa_thread_attr_t thread_attr;
	thread_attr.name = "lisa_evs_auth_thread";
	thread_attr.stack_size = REQUEST_STACK_SIZE;
	thread_attr.priority = LISA_OS_PRIORITY_NORMAL; // OS_PRIORITY_NORMAL
	g_request_thread = lisa_thread_create(&thread_attr, _auth_request_accesstoken_thread, (void *)auth_context);
	if (g_request_thread == NULL) {
		if (auth_context) {
			if (auth_context->m_refresh_token) {lisa_mem_free(auth_context->m_refresh_token);}
			lisa_mem_free(auth_context);
		}
		LISA_LOGE(TAG, "lisa auth thread_create failed");
		return -1;
	}
	return 0;
}
