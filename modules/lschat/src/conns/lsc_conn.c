#define TAG "lsc_conn"
#include "lsc_conn.h"
#include "lsc_errno.h"
#include "lsc_config.h"
#include "lsc_base64.h"
#include "lsc_common.h"
#include "lisa_http.h"
#include "lisa_websocket.h"
#include "lisa_sntp.h"
#include "cJSON.h"
#include "mbedtls/md5.h"

#include "lisa_log.h"
#include "lisa_mem.h"

__attribute__((weak)) const char *lsc_get_firmware_type(void);
__attribute__((weak)) const char *lsc_get_firmware_version(void);

static lsc_conn_t *g_lsc_conn_obj = NULL;

int lsc_set_device_mode(lsc_device_mode_t device_mode)
{
	if (g_lsc_conn_obj != NULL) {
		g_lsc_conn_obj->device_mode = device_mode;
		return LSC_OK;
	} else {
		LISA_NLOGE("lsc_set_device_mode err ,g_lsc_conn_obj is NULL");
		return LSC_ERR;
	}
}

static lsc_device_mode_t lsc_get_device_mode(void)
{
	return g_lsc_conn_obj->device_mode;
}

static const char *lsc_get_host_for_mode(lsc_conn_t *conn, lsc_device_mode_t device_mode)
{
	switch (device_mode) {
	case LSC_DEVICE_MODE_STAGING:
		return LSC_GET_HOST_STAGING_FROM_SERVER(conn->server_config);
	case LSC_DEVICE_MODE_INTEGRATION:
		return LSC_GET_HOST_INTEGRATION_FROM_SERVER(conn->server_config);
	case LSC_DEVICE_MODE_PROD:
	default:
		return LSC_GET_HOST_FROM_SERVER(conn->server_config);
	}
}

static char *lsc_get_token_url_for_mode(lsc_conn_t *conn, lsc_device_mode_t device_mode)
{
	switch (device_mode) {
	case LSC_DEVICE_MODE_STAGING:
		return LSC_GET_TOKEN_URL_STAGING_FROM_SERVER(conn->server_config);
	case LSC_DEVICE_MODE_INTEGRATION:
		return LSC_GET_TOKEN_URL_INTEGRATION_FROM_SERVER(conn->server_config);
	case LSC_DEVICE_MODE_PROD:
	default:
		return LSC_GET_TOKEN_URL_FROM_SERVER(conn->server_config);
	}
}

static int conn_update_auth_header(char *token)
{
	lsc_conn_t *conn = g_lsc_conn_obj;
	uint32_t token_size = strlen(token) + 1;
	uint32_t header_size = token_size + strlen("\r\nAuthorization: Bearer ");

	if (conn->auth_header) {
		conn->auth_header = lisa_mem_realloc(conn->auth_header, header_size);
	} else {
		conn->auth_header = lisa_mem_calloc(1, header_size);
	}
	CHECK_COND_RETURN_VAL(conn->auth_header, LSC_NO_MEM, "no mem");

	sprintf(conn->auth_header, "\r\nAuthorization: Bearer %s", token);

	LISA_NLOGI("update auth header: %s", conn->auth_header);

	return LSC_OK;
}

static void conn_clear_auth_header(void)
{
	lsc_conn_t *conn = g_lsc_conn_obj;

	if (conn->auth_header) {
		lisa_mem_free(conn->auth_header);
	}

	conn->auth_header = NULL;
}

static char *conn_generate_url_from_malloc(void)
{
	cJSON *params = cJSON_CreateObject();
	CHECK_COND_RETURN_VAL(params, NULL, "no mem");

	cJSON_AddStringToObject(params, "scene", "main");

#if CONFIG_LS_CHAT_MCP
	cJSON_AddBoolToObject(params, "mcp", true);
#endif

#if CONFIG_LS_CHAT_MCP_V2
	cJSON_AddStringToObject(params, "tool_protocol_version", "v2");
#endif

	cJSON_AddStringToObject(params, "type", "fullduplex");

	const char *firmware_type = lsc_get_firmware_type ? lsc_get_firmware_type() : NULL;
	const char *firmware_version = lsc_get_firmware_version ? lsc_get_firmware_version() : NULL;
	if (firmware_type && firmware_version && firmware_type[0] != '\0' && firmware_version[0] != '\0') {
		cJSON *firmware_info = cJSON_CreateObject();
		if (firmware_info) {
			cJSON_AddStringToObject(firmware_info, "type", firmware_type);
			cJSON_AddStringToObject(firmware_info, "version", firmware_version);
			cJSON_AddItemToObject(params, "firmware_info", firmware_info);
		}
	}

	char *params_json = cJSON_PrintUnformatted(params);
	cJSON_Delete(params);
	CHECK_COND_RETURN_VAL(params_json, NULL, "params json null");

	LISA_NLOGI("params: %s", params_json);

	char *params_base64 = lsc_base64_encode(params_json);
	cJSON_free(params_json);
	CHECK_COND_RETURN_VAL(params_base64, NULL, "encode base64 error");

	const char *ws_base_path = "/v1/interaction?param=";

	char *ws_path = lisa_mem_calloc(1, strlen(ws_base_path) + strlen(params_base64) + 1);
	CHECK_COND_RETURN_VAL(ws_path, NULL, "no mem");

	sprintf(ws_path, "%s%s", ws_base_path, params_base64);
	lisa_mem_free(params_base64);

	LISA_NLOGI("------wsurl: %s", ws_path);
	return ws_path;
}

static void http_token_on_data(lisa_http_data_t *data)
{
	lsc_conn_t *conn = g_lsc_conn_obj;

	cJSON *json_root = cJSON_Parse(data->buf);
	if (!json_root) {
		LISA_NLOGI("aiui token json parse failed\n");
		lisa_evt_publisher_publish(conn->evt_cb_list, CONN_AUTH_FAILD, NULL, 0);
		goto _err;
	}

	cJSON *cj_token = cJSON_GetObjectItem(json_root, "token");
	if (cj_token == NULL) {
		LISA_NLOGI("parse token failed\n");
		char *temp = cJSON_Print(json_root);
		LISA_NLOGI("auth : %s", temp);
		cJSON_free(temp);
		lisa_evt_publisher_publish(conn->evt_cb_list, CONN_AUTH_FAILD, NULL, 0);
		goto _err;
	}

	int token_len = strlen(cj_token->valuestring);
	char *aiui_token = lisa_mem_calloc(1, token_len + 1);
	CHECK_COND_GOTO(aiui_token, _err, "no mem");

	memcpy(aiui_token, cj_token->valuestring, token_len);

	lisa_evt_publisher_publish(conn->evt_cb_list, CONN_AUTH_SUCESS, (void *)aiui_token, token_len);

	if (aiui_token) {
		lisa_mem_free(aiui_token);
	}

_err:
	if (json_root) {
		cJSON_Delete(json_root);
	}
}

static void *http_client_get_headers(void)
{
#define HTTP_REQ_HEADER "Content-Type: application/json"
	return HTTP_REQ_HEADER;
}

static void _ws_data_cb(lisa_ws_data_t *data)
{
	lsc_conn_t *conn = g_lsc_conn_obj;

	cJSON *root = cJSON_Parse(data->buf);
	if (root == NULL) {
		LISA_NLOGE("[%s] ws data cjson parse faild", __FUNCTION__);
		return;
	}
	LISA_NLOGI("ws data: %d, %s", data->len, (char *)data->buf);
	lisa_evt_publisher_publish(conn->evt_cb_list, CONN_DATA_CJSON, (void *)root, 0);
	if (root) {
		cJSON_Delete(root);
	}
}

static void _ws_event_cb(lisa_ws_event_t *event)
{
	lsc_conn_t *conn = g_lsc_conn_obj;
	LISA_NLOGI("[%s] evt:%d", __FUNCTION__, event->what);
	switch (event->what) {
	case LISA_WS_ON_CONNECTED:
		lisa_evt_publisher_publish(conn->evt_cb_list, CONN_CONNECTED, NULL, 0);
		break;
	case LISA_WS_ON_CONNECTING:
		lisa_evt_publisher_publish(conn->evt_cb_list, CONN_CONNECTING, NULL, 0);
		break;
	case LISA_WS_ON_DISCONNECTED:
		lisa_evt_publisher_publish(conn->evt_cb_list, CONN_DISCONNECTED, NULL, 0);
		break;
	default:
		break;
	}
}

static int conn_connect(char *token)
{
	int ret;
	lsc_device_mode_t device_mode = LSC_DEVICE_MODE_PROD;

	lsc_conn_t *conn = g_lsc_conn_obj;
	CHECK_COND_RETURN_VAL(conn, LSC_INVALID_STATE, "conn not init");

	ret = conn_update_auth_header(token);
	CHECK_COND_RETURN_VAL(ret == LSC_OK, LSC_INVALID_STATE, "update auth header faild");

	char *ws_url = conn_generate_url_from_malloc();
	CHECK_COND_GOTO(ws_url, _err, "gen url faild");

	lisa_ws_request_t req;
	memset((void *)&req, 0, sizeof(lisa_ws_request_t));
	req.timeout = 10;
	req.on_data = _ws_data_cb;
	req.on_event = _ws_event_cb;
	req.user = NULL;
	req.scheme = LSC_GET_SCHEME_FROM_SERVER(conn->server_config);
	device_mode = lsc_get_device_mode();
	req.host = (char *)lsc_get_host_for_mode(conn, device_mode);

	LISA_NLOGI("device mode: %d, host: %s", device_mode, req.host);

	req.path = ws_url;
	req.port = LSC_GET_PORT_FROM_SERVER(conn->server_config);
	req.extra_header = conn->auth_header;
	ret = lisa_ws_cfg_set(conn->ws_hdl, &req);
	CHECK_COND_GOTO(ret == LISA_WS_OK, _err, "lisa ws config failed");

	if (ws_url) {
		lisa_mem_free(ws_url);
		ws_url = NULL;
	}

	ret = lisa_ws_connect(conn->ws_hdl);
	CHECK_COND_GOTO(ret == LISA_WS_OK, _err, "ws connect error");

	return LSC_OK;
_err:
	if (ws_url) {
		lisa_mem_free(ws_url);
	}

	conn_clear_auth_header();

	if (conn->ws_hdl) {
		lisa_ws_cleanup(conn->ws_hdl);
		conn->ws_hdl = NULL;
	}
	return LSC_ERR;
}

static int conn_disconnect(void)
{
	lsc_conn_t *conn = g_lsc_conn_obj;
	CHECK_COND_RETURN_VAL(conn, LSC_INVALID_STATE, "conn not init");
	CHECK_COND_RETURN_VAL(conn->ws_hdl, LSC_INVALID_STATE, "conn ws not init");

	conn_clear_auth_header();

	lisa_ws_disconnect(conn->ws_hdl);

	return LSC_OK;
}

static int conn_add_evt_callback(conn_event_cb_t cb, conn_event_e evt, void *usr)
{
	lsc_conn_t *conn = g_lsc_conn_obj;
	CHECK_COND_RETURN_VAL(conn, LSC_INVALID_STATE, "conn not init");
	CHECK_COND_RETURN_VAL(conn->evt_cb_list, LSC_INVALID_STATE, "conn cb list not create");

	int ret = lisa_evt_publisher_evt_add(conn->evt_cb_list, evt, (lisa_evt_publisher_cb_t)cb, usr);
	if (ret) {
		return LSC_ERR;
	}

	return LSC_OK;
}

static int conn_remove_evt_callback(conn_event_cb_t cb)
{
	lsc_conn_t *conn = g_lsc_conn_obj;
	CHECK_COND_RETURN_VAL(conn, LSC_INVALID_STATE, "conn not init");
	CHECK_COND_RETURN_VAL(conn->evt_cb_list, LSC_INVALID_STATE, "conn cb list not create");

	lisa_evt_publisher_cb_remove(conn->evt_cb_list, (lisa_evt_publisher_cb_t)cb);

	return LSC_OK;
}

static int conn_send_bin(const uint8_t *data, uint32_t size)
{
	lsc_conn_t *conn = g_lsc_conn_obj;
	CHECK_COND_RETURN_VAL(conn, LSC_INVALID_STATE, "conn not init");
	CHECK_COND_RETURN_VAL(conn->ws_hdl, LSC_INVALID_STATE, "conn ws not create");

	int ret = lisa_ws_send_binary(conn->ws_hdl, (const void *)data, size);
	if (ret) {
		return LSC_ERR;
	}

	return LSC_OK;
}

static int conn_send_text(char *text)
{
	lsc_conn_t *conn = g_lsc_conn_obj;
	CHECK_COND_RETURN_VAL(conn, LSC_INVALID_STATE, "conn not init");
	CHECK_COND_RETURN_VAL(conn->ws_hdl, LSC_INVALID_STATE, "conn ws not create");

	int ret = lisa_ws_send_text(conn->ws_hdl, (const uint8_t *)text);
	if (ret) {
		return LSC_ERR;
	}

	return LSC_OK;
}

static int conn_auth(char *device_id, char *product_id, char *secret_id, char *extra_param)
{
	int ret = 0;
	lsc_device_mode_t device_mode = LSC_DEVICE_MODE_PROD;
	LISA_NLOGI("[%s] dev_id:%s pro_id:%s ser_id:%s extra:%s", __FUNCTION__, device_id, product_id, secret_id,
		   extra_param ? extra_param : "NULL");

	struct lisa_sntp_time time = {0};
	const char *servers[] = {"ntp.aliyun.com", "ntp.tencent.com", "ntp.ntsc.ac.cn"};
	int err = lisa_sntp_query(servers, sizeof(servers) / sizeof(char *), 500, &time);
	CHECK_COND_RETURN_VAL(err == 0, LSC_ERR, "sntp query faild");

	// 数据格式化后最大10位数字，再加上结束符
	char current_time[12] = {0};
	sprintf(current_time, "%u", (uint32_t)time.sec);

	char *origin_product_ntp = lisa_mem_calloc(1, strlen(secret_id) + strlen(current_time) + strlen(device_id) + 1);
	CHECK_COND_RETURN_VAL(origin_product_ntp, LSC_NO_MEM, "no mem");

	strcat(origin_product_ntp, secret_id);
	strcat(origin_product_ntp, device_id);
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

	// TODO:检查内存溢出
	char req_body[256] = {'\0'};
	if (extra_param && strlen(extra_param) > 0) {
		sprintf(req_body,
			"{\"productId\": \"%s\",\"deviceId\": \"%s\",\"curtime\": %s, \"checksum\": \"%s\", \"code\": "
			"\"%s\"}",
			product_id, device_id, current_time, md5_hex_string, extra_param);
	} else {
		sprintf(req_body, "{\"productId\": \"%s\",\"deviceId\": \"%s\",\"curtime\": %s, \"checksum\": \"%s\"}",
			product_id, device_id, current_time, md5_hex_string);
	}

	lsc_conn_t *conn = g_lsc_conn_obj;

	lisa_http_request_t req = {0};
	req.method = LISA_HTTP_POST;
	device_mode = lsc_get_device_mode();
	// 根据环境选择不同的TOKEN URL
	req.url = lsc_get_token_url_for_mode(conn, device_mode);
	req.timeout = 10;
	req.on_data = http_token_on_data;
	req.body = req_body;
	req.body_len = strlen(req.body);
	req.headers = (uint8_t *)http_client_get_headers;

	LISA_NLOGI("reqbody: %s\r\n", req_body);

	lisa_http_t *http = lisa_http_init(&req);
	CHECK_COND_GOTO(http, _err, "http init faild");

	ret = lisa_http_perform(http);
	CHECK_COND_GOTO(ret == LISA_HTTP_OK, _err, "http perform faild(ret=%d)", ret);

	lisa_http_cleanup(http);

	if (origin_product_ntp) {
		lisa_mem_free(origin_product_ntp);
	}

	return LSC_OK;

_err:
	if (http) {
		lisa_http_cleanup(http);
	}
	if (origin_product_ntp) {
		lisa_mem_free(origin_product_ntp);
	}
	return LSC_ERR;
}

lsc_conn_t *lsc_conn_create(lsc_server_config_t *server_config)
{
	lsc_conn_t *conn = lisa_mem_calloc(1, sizeof(lsc_conn_t));
	CHECK_COND_GOTO(conn, _err, "no mem");

	conn->server_config = server_config;  // 保存服务器配置指针

	conn->evt_cb_list = lisa_evt_publisher_new();
	CHECK_COND_GOTO(conn->evt_cb_list, _err, "no mem");

	conn->add_evt_callback = conn_add_evt_callback;
	conn->remove_evt_callback = conn_remove_evt_callback;
	conn->connect = conn_connect;
	conn->disconnect = conn_disconnect;
	conn->send_bin = conn_send_bin;
	conn->send_text = conn_send_text;
	conn->auth = conn_auth;

	g_lsc_conn_obj = conn;

	conn->ws_hdl = lisa_ws_new();
	if (conn->ws_hdl == NULL) {
		LISA_NLOGE("lisa websocket new failed");
		goto _err;
	}

	return conn;
_err:
	if (conn) {
		lisa_mem_free(conn);
	}

	if (conn->evt_cb_list) {
		lisa_evt_publisher_destroy(conn->evt_cb_list);
	}

	return NULL;
}

int lsc_conn_destroy(lsc_conn_t *hdl)
{
	lsc_conn_t *conn = g_lsc_conn_obj;
	CHECK_COND_RETURN_VAL(hdl, LSC_INVALID_PARAM, " ");
	CHECK_COND_RETURN_VAL(hdl == conn, LSC_INVALID_PARAM, " ");

	if (conn->ws_hdl) {
		lisa_ws_delete(conn->ws_hdl);
		conn->ws_hdl = NULL;
	}

	if (conn->evt_cb_list) {
		lisa_evt_publisher_destroy(conn->evt_cb_list);
	}

	conn_clear_auth_header();

	if (hdl) {
		lisa_mem_free(g_lsc_conn_obj);
		g_lsc_conn_obj = NULL;
	}

	return LSC_OK;
}
