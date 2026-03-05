#define TAG "lsc_core"
#include <stdbool.h>
#include <stdio.h>
#include "lsc.h"
#include "lsc_errno.h"
#include "lsc_conn.h"
#include "lsc_common.h"
#include "lsc_sessions_core.h"
#include "lisa_evt_pub.h"
#include "lisa_thread.h"
#include "lisa_semaphore.h"
#include "lisa_log.h"
#include "lisa_http.h"
#include "cJSON.h"
#include "lsc_config.h"
typedef struct {
	lsc_config_t *config;
	lsc_conn_t *conn;
	lisa_evt_publisher_t cb_list;
	char *auth_token;
	char *auth_header;
	bool music_active;
	lisa_thread_t *reconnect_thread;
	lsc_event_e statu;
	lisa_semaphore_t *connected_sem;
} lsc_t;

static lsc_t *g_lsc_obj = NULL;
static void *_http_music_req_url_headers(void);

static bool lsc_if_got_token(void)
{
	lsc_t *lsc = g_lsc_obj;

	return lsc->auth_token ? true : false;
}

static int lsc_clear_token(void)
{
	lsc_t *lsc = g_lsc_obj;
	if (lsc->auth_token) {
		lisa_mem_free(lsc->auth_token);
		lsc->auth_token = NULL;
	}

	if (lsc->auth_header) {
		lisa_mem_free(lsc->auth_header);
		lsc->auth_header = NULL;
	}

	return LSC_OK;
}

static int lsc_update_token(char *token)
{
	lsc_t *lsc = g_lsc_obj;
	CHECK_COND_RETURN_VAL(lsc, LSC_INVALID_STATE, "lsc not create");
	CHECK_COND_RETURN_VAL(token, LSC_INVALID_PARAM, " ");

	uint32_t size = strlen(token) + 1;
	uint32_t header_size = size + strlen("Authorization: Bearer ") + strlen("\r\n");

	if (lsc->auth_token) {
		lsc->auth_token = lisa_mem_realloc(lsc->auth_token, size);
	} else {
		lsc->auth_token = lisa_mem_calloc(1, size);
	}

	CHECK_COND_RETURN_VAL(lsc->auth_token, LSC_NO_MEM, "no mem");

	strncpy(lsc->auth_token, token, size);

	lsc->auth_header = lisa_mem_alloc(header_size);
	CHECK_COND_GOTO(lsc->auth_header, _err, "no mem");

	sprintf(lsc->auth_header, "Authorization: Bearer %s\r\n", lsc->auth_token);

	LISA_NLOGI("[%s] (size=%d)token:%s", __FUNCTION__, size, token);

	return LSC_OK;
_err:
	if (lsc->auth_token) {
		lsc->auth_token = NULL;
		lisa_mem_free(lsc->auth_token);
	}
	return LSC_ERR;
}

static void lsc_set_status(lsc_event_e evt)
{
	lsc_t *lsc = g_lsc_obj;
	lsc->statu = evt;
}

static lsc_event_e lsc_get_status(void)
{
	lsc_t *lsc = g_lsc_obj;
	return lsc->statu;
}

static void _lsc_core_conn_evt_cb(conn_event_e evt, void *data, uint32_t size, void *usr)
{
	lsc_t *lsc = g_lsc_obj;

	LISA_NLOGD("[%s] evt:%s ", __FUNCTION__, STRINGS_CONN_EVT(evt));

	switch (evt) {
	case CONN_AUTH_SUCESS:
		lsc_update_token(data);
		lsc_set_status(LSC_GOT_TOKEN);
		lisa_evt_publisher_publish(lsc->cb_list, LSC_GOT_TOKEN, (void *)data, size);
		break;
	case CONN_AUTH_FAILD:
		lsc_clear_token();
		lsc_set_status(LSC_CLOUD_AUTH_FAILD);
		lisa_evt_publisher_publish(lsc->cb_list, LSC_CLOUD_AUTH_FAILD, NULL, 0);
		break;
	case CONN_CONNECTED:
		// 链路层已连接
		//  lsc_set_status(LSC_CONNECTED);
		//  lisa_evt_publisher_publish(lsc->cb_list, LSC_CONNECTED, NULL, 0);
		if (lsc->connected_sem) {
			lisa_semaphore_give(lsc->connected_sem);
		}
		break;
	case CONN_DISCONNECTED:
		lsc_set_status(LSC_DISCONNECTED);
		lisa_evt_publisher_publish(lsc->cb_list, LSC_DISCONNECTED, NULL, 0);
		break;
	case CONN_CONNECTING:
		lsc_set_status(LSC_CONNECTING);
		lisa_evt_publisher_publish(lsc->cb_list, LSC_CONNECTING, NULL, 0);
		break;
	case CONN_DATA_CJSON: {
		cJSON *root = (cJSON *)data;
		CHECK_COND_RETURN(root, "is NULL");

		cJSON *root_action = cJSON_GetObjectItem(root, "action");
		if (root_action) {
			if (!strcmp(root_action->valuestring, "connected")) {
				lsc_set_status(LSC_CONNECTED);
				lisa_evt_publisher_publish(lsc->cb_list, LSC_CONNECTED, lsc->conn, 0);
			}
		}
		lisa_evt_publisher_publish(lsc->cb_list, LSC_DATA_RECEIVED, data, size);
	} break;
	default:
		break;
	}
}

static void clear_config(lsc_config_t *cfg)
{
	if (cfg) {
		if (cfg->device_id) {
			lisa_mem_free(cfg->device_id);
			cfg->device_id = NULL;
		}
		if (cfg->product_id) {
			lisa_mem_free(cfg->product_id);
			cfg->product_id = NULL;
		}
		if (cfg->secret_id) {
			lisa_mem_free(cfg->secret_id);
			cfg->secret_id = NULL;
		}
		if (cfg->extra_param) {
			lisa_mem_free(cfg->extra_param);
			cfg->extra_param = NULL;
		}
		if (cfg->token) {
			lisa_mem_free(cfg->token);
			cfg->token = NULL;
		}
		if (cfg->server_config) {
			if (cfg->server_config->host) {
				lisa_mem_free((void *)cfg->server_config->host);
			}
			if (cfg->server_config->host_staging) {
				lisa_mem_free((void *)cfg->server_config->host_staging);
			}
			if (cfg->server_config->host_integration) {
				lisa_mem_free((void *)cfg->server_config->host_integration);
			}
			if (cfg->server_config->token_url) {
				lisa_mem_free((void *)cfg->server_config->token_url);
			}
			if (cfg->server_config->token_url_staging) {
				lisa_mem_free((void *)cfg->server_config->token_url_staging);
			}
			if (cfg->server_config->token_url_integration) {
				lisa_mem_free((void *)cfg->server_config->token_url_integration);
			}
			if (cfg->server_config->music_active_url) {
				lisa_mem_free((void *)cfg->server_config->music_active_url);
			}
			if (cfg->server_config->music_tranlink_url) {
				lisa_mem_free((void *)cfg->server_config->music_tranlink_url);
			}
			if (cfg->server_config->port) {
				lisa_mem_free((void *)cfg->server_config->port);
			}
			if (cfg->server_config->scheme) {
				lisa_mem_free((void *)cfg->server_config->scheme);
			}
			lisa_mem_free(cfg->server_config);
			cfg->server_config = NULL;
		}
		lisa_mem_free(cfg);
	}
}

static lsc_config_t *update_config_from_malloc(lsc_config_t *cfg)
{
	lsc_config_t *result = lisa_mem_calloc(1, sizeof(lsc_config_t));
	CHECK_COND_GOTO(result, _err, "no mem");

	if (cfg->device_id) {
		result->device_id = lisa_mem_calloc(1, strlen(cfg->device_id) + 1);
		CHECK_COND_GOTO(result->device_id, _err, "no mem");
		strcpy(result->device_id, cfg->device_id);
	}

	if (cfg->product_id) {
		result->product_id = lisa_mem_calloc(1, strlen(cfg->product_id) + 1);
		CHECK_COND_GOTO(result->product_id, _err, "no mem");
		strcpy(result->product_id, cfg->product_id);
	}

	if (cfg->secret_id) {
		result->secret_id = lisa_mem_calloc(1, strlen(cfg->secret_id) + 1);
		CHECK_COND_GOTO(result->secret_id, _err, "no mem");
		strcpy(result->secret_id, cfg->secret_id);
	}

	if (cfg->extra_param) {
		result->extra_param = lisa_mem_calloc(1, strlen(cfg->extra_param) + 1);
		CHECK_COND_GOTO(result->extra_param, _err, "no mem");
		strcpy(result->extra_param, cfg->extra_param);
	}

	if (cfg->token) {
		result->token = lisa_mem_calloc(1, strlen(cfg->token) + 1);
		CHECK_COND_GOTO(result->token, _err, "no mem");
		strcpy(result->token, cfg->token);
	}

	result->device_mode = cfg->device_mode;
	result->if_auto_reconn = cfg->if_auto_reconn;
	result->reconn_interval_ms = result->if_auto_reconn ? cfg->reconn_interval_ms : 0;

	/* 深拷贝 server_config */
	if (cfg->server_config) {
		result->server_config = lisa_mem_calloc(1, sizeof(lsc_server_config_t));
		CHECK_COND_GOTO(result->server_config, _err, "no mem");

		/* 深拷贝所有字符串字段 */
		if (cfg->server_config->host) {
			result->server_config->host = lisa_mem_calloc(1, strlen(cfg->server_config->host) + 1);
			CHECK_COND_GOTO(result->server_config->host, _err, "no mem");
			strcpy((char *)result->server_config->host, cfg->server_config->host);
		}

		if (cfg->server_config->host_staging) {
			result->server_config->host_staging = lisa_mem_calloc(1, strlen(cfg->server_config->host_staging) + 1);
			CHECK_COND_GOTO(result->server_config->host_staging, _err, "no mem");
			strcpy((char *)result->server_config->host_staging, cfg->server_config->host_staging);
		}
		if (cfg->server_config->host_integration) {
			result->server_config->host_integration =
				lisa_mem_calloc(1, strlen(cfg->server_config->host_integration) + 1);
			CHECK_COND_GOTO(result->server_config->host_integration, _err, "no mem");
			strcpy((char *)result->server_config->host_integration, cfg->server_config->host_integration);
		}

		if (cfg->server_config->token_url) {
			result->server_config->token_url = lisa_mem_calloc(1, strlen(cfg->server_config->token_url) + 1);
			CHECK_COND_GOTO(result->server_config->token_url, _err, "no mem");
			strcpy((char *)result->server_config->token_url, cfg->server_config->token_url);
		}

		if (cfg->server_config->token_url_staging) {
			result->server_config->token_url_staging =
				lisa_mem_calloc(1, strlen(cfg->server_config->token_url_staging) + 1);
			CHECK_COND_GOTO(result->server_config->token_url_staging, _err, "no mem");
			strcpy((char *)result->server_config->token_url_staging, cfg->server_config->token_url_staging);
		}
		if (cfg->server_config->token_url_integration) {
			result->server_config->token_url_integration =
				lisa_mem_calloc(1, strlen(cfg->server_config->token_url_integration) + 1);
			CHECK_COND_GOTO(result->server_config->token_url_integration, _err, "no mem");
			strcpy((char *)result->server_config->token_url_integration,
			       cfg->server_config->token_url_integration);
		}

		if (cfg->server_config->music_active_url) {
			result->server_config->music_active_url = lisa_mem_calloc(1, strlen(cfg->server_config->music_active_url) + 1);
			CHECK_COND_GOTO(result->server_config->music_active_url, _err, "no mem");
			strcpy((char *)result->server_config->music_active_url, cfg->server_config->music_active_url);
		}

		if (cfg->server_config->music_tranlink_url) {
			result->server_config->music_tranlink_url = lisa_mem_calloc(1, strlen(cfg->server_config->music_tranlink_url) + 1);
			CHECK_COND_GOTO(result->server_config->music_tranlink_url, _err, "no mem");
			strcpy((char *)result->server_config->music_tranlink_url, cfg->server_config->music_tranlink_url);
		}

		if (cfg->server_config->port) {
			result->server_config->port = lisa_mem_calloc(1, strlen(cfg->server_config->port) + 1);
			CHECK_COND_GOTO(result->server_config->port, _err, "no mem");
			strcpy((char *)result->server_config->port, cfg->server_config->port);
		}

		if (cfg->server_config->scheme) {
			result->server_config->scheme = lisa_mem_calloc(1, strlen(cfg->server_config->scheme) + 1);
			CHECK_COND_GOTO(result->server_config->scheme, _err, "no mem");
			strcpy((char *)result->server_config->scheme, cfg->server_config->scheme);
		}
	}

	return result;

_err:
	if (result) {
		if (result->device_id) {
			lisa_mem_free(result->device_id);
			result->device_id = NULL;
		}
		if (result->product_id) {
			lisa_mem_free(result->product_id);
			result->product_id = NULL;
		}
		if (result->secret_id) {
			lisa_mem_free(result->secret_id);
			result->secret_id = NULL;
		}
		if (result->extra_param) {
			lisa_mem_free(result->extra_param);
			result->extra_param = NULL;
		}
		if (result->token) {
			lisa_mem_free(result->token);
			result->token = NULL;
		}
		if (result->server_config) {
			if (result->server_config->host) {
				lisa_mem_free((void *)result->server_config->host);
			}
			if (result->server_config->host_staging) {
				lisa_mem_free((void *)result->server_config->host_staging);
			}
			if (result->server_config->host_integration) {
				lisa_mem_free((void *)result->server_config->host_integration);
			}
			if (result->server_config->token_url) {
				lisa_mem_free((void *)result->server_config->token_url);
			}
			if (result->server_config->token_url_staging) {
				lisa_mem_free((void *)result->server_config->token_url_staging);
			}
			if (result->server_config->token_url_integration) {
				lisa_mem_free((void *)result->server_config->token_url_integration);
			}
			if (result->server_config->music_active_url) {
				lisa_mem_free((void *)result->server_config->music_active_url);
			}
			if (result->server_config->music_tranlink_url) {
				lisa_mem_free((void *)result->server_config->music_tranlink_url);
			}
			if (result->server_config->port) {
				lisa_mem_free((void *)result->server_config->port);
			}
			if (result->server_config->scheme) {
				lisa_mem_free((void *)result->server_config->scheme);
			}
			lisa_mem_free(result->server_config);
			result->server_config = NULL;
		}
		lisa_mem_free(result);
	}

	return NULL;
}

static void lsc_reconnect_thread(void *param)
{
	lsc_t *lsc = g_lsc_obj;
	int ret = LSC_OK;

	while (1) {
		if (lsc_get_status() == LSC_DISCONNECTED) {
			LISA_NLOGI("lsc_reconnect_thread, reconnect start");
			if (lsc_if_got_token() == false) {
				ret = lsc->conn->auth(lsc->config->device_id, lsc->config->product_id,
						      lsc->config->secret_id, lsc->config->extra_param);
				if (ret) {
					LISA_NLOGE("lsc auth opt faild(ret = %d)", ret);
				}

				if (lsc_if_got_token() == false) {
					LISA_NLOGE("lsc auth faild !");
				}
			}
			ret = lsc->conn->connect(lsc->auth_token);
			if (ret == 0) {
				LISA_NLOGI("lsc waiting connected...");
				ret = lisa_semaphore_take(lsc->connected_sem, 10 * 1000);
				if (ret) {
					LISA_NLOGE("lsc waiting connected sem timeout");
					lsc->conn->disconnect();
				} else {
					LISA_NLOGI("lsc reconnected successfully");
				}
			} else {
				LISA_NLOGE("lsc connect faild(ret = %d)", ret);
			}
		}
		lisa_thread_mdelay(lsc->config->reconn_interval_ms);
	}
}

int lsc_set_config(lsc_config_t *cfg)
{
	lsc_t *lsc = g_lsc_obj;
	if (cfg == NULL || g_lsc_obj == NULL) {
		return LSC_ERR;
	}

	clear_config(lsc->config);
	lsc->config = update_config_from_malloc(cfg);

	if (lsc->config == NULL) {
		return LSC_ERR;
	}
	return LSC_OK;
}

int lsc_init(lsc_config_t *cfg)
{
	int ret;
	lsc_t *lsc = NULL;

	LISA_NLOGI("lschat version: v%s, commit:%s", LSCHAT_VERSION, LSCHAT_GIT_COMMIT);

	CHECK_COND_RETURN_VAL(g_lsc_obj == NULL, LSC_INVALID_STATE, "lsc already initialized");
	CHECK_COND_RETURN_VAL(cfg, LSC_INVALID_PARAM, "cfg is null");
	CHECK_COND_RETURN_VAL(cfg->device_id, LSC_INVALID_PARAM, "device_id is null");
	CHECK_COND_RETURN_VAL(cfg->product_id, LSC_INVALID_PARAM, "product_id is null");
	CHECK_COND_RETURN_VAL(cfg->secret_id, LSC_INVALID_PARAM, "secret_id is null");

	lsc = lisa_mem_calloc(1, sizeof(lsc_t));
	CHECK_COND_GOTO(lsc, _err, "no mem");

	g_lsc_obj = lsc;

	lsc->cb_list = lisa_evt_publisher_new();
	CHECK_COND_GOTO(lsc->cb_list, _err, "lisa cb list create faild");

	lsc->config = update_config_from_malloc(cfg);
	CHECK_COND_GOTO(lsc->config, _err, "update config faild");

	if (lsc->config->token) {
		ret = lsc_update_token(lsc->config->token);
		CHECK_COND_GOTO(ret == LSC_OK, _err, "update token faild");
	}

	lsc->conn = lsc_conn_create(lsc->config->server_config);
	CHECK_COND_GOTO(lsc->conn, _err, "conn create faild");

	lsc->conn->add_evt_callback(_lsc_core_conn_evt_cb,
				    CONN_CONNECTING | CONN_CONNECTED | CONN_DISCONNECTED | CONN_AUTH_SUCESS |
					    CONN_AUTH_FAILD | CONN_DATA_CJSON,
				    NULL);

	lsc->connected_sem = NULL;

	if (cfg->if_auto_reconn) {
		lsc->connected_sem = lisa_semaphore_create(1);
		if (lsc->connected_sem == NULL) {
			LISA_NLOGE("connecting sem create failed");
			goto _err;
		}

		lisa_thread_attr_t thread_attr;
		thread_attr.name = "lsc_reconnect";
		thread_attr.stack_size = 4 * 1024;
		thread_attr.priority = LISA_OS_PRIORITY_ABOVE_NORMAL;
		lsc->reconnect_thread = lisa_thread_create(&thread_attr, lsc_reconnect_thread, NULL);
		CHECK_COND_GOTO(lsc->reconnect_thread, _err, "lsc_reconnect thread cretae faild");
	}

	lsc->statu = LSC_NONE;

	ret = sessions_core_init(lsc->conn);
	CHECK_COND_GOTO(ret == LSC_OK, _err, "sessions core init faild");

	lsc_set_device_mode(lsc->config->device_mode);

	return LSC_OK;
_err:
	if (lsc) {

		if (lsc->connected_sem) {
			lisa_semaphore_delete(lsc->connected_sem);
		}

		if (lsc->cb_list) {
			lisa_evt_publisher_destroy(lsc->cb_list);
		}
		if (lsc->config) {
			clear_config(lsc->config);
		}
		if (lsc->conn) {
			lsc_conn_destroy(lsc->conn);
			lsc->conn = NULL;
		}
		if (lsc->reconnect_thread) {
			lisa_thread_delete(lsc->reconnect_thread);
		}
		lsc_clear_token();
		lisa_mem_free(lsc);
		g_lsc_obj = NULL;
	}
	return LSC_ERR;
}

int lsc_remove_callback(lsc_event_cb_t cb)
{
	lsc_t *lsc = g_lsc_obj;
	CHECK_COND_RETURN_VAL(lsc, LSC_INVALID_STATE, "lsc not create");
	CHECK_COND_RETURN_VAL(lsc->cb_list, LSC_INVALID_STATE, "cb list not create");

	lisa_evt_publisher_cb_remove(lsc->cb_list, (lisa_evt_publisher_cb_t)cb);

	return LSC_OK;
}

int lsc_add_callback(lsc_event_e evt, lsc_event_cb_t cb, void *usr)
{
	lsc_t *lsc = g_lsc_obj;
	CHECK_COND_RETURN_VAL(lsc, LSC_INVALID_STATE, "lsc not create");
	CHECK_COND_RETURN_VAL(lsc->cb_list, LSC_INVALID_STATE, "cb list not create");

	int ret = lisa_evt_publisher_evt_add(lsc->cb_list, evt, (lisa_evt_publisher_cb_t)cb, usr);
	if (ret) {
		return LSC_ERR;
	}

	return LSC_OK;
}

int lsc_connect(void)
{
	lsc_t *lsc = g_lsc_obj;
	int ret = LSC_OK;
	CHECK_COND_RETURN_VAL(lsc, LSC_INVALID_STATE, "lsc not create");

	LISA_NLOGI("lsc connect");

	if (lsc_if_got_token() == false) {
		ret = lsc->conn->auth(lsc->config->device_id, lsc->config->product_id, lsc->config->secret_id,
				      lsc->config->extra_param);
		if (ret) {
			LISA_NLOGE("lsc auth opt faild(ret = %d)", ret);
			return LSC_ERR;
		}

		if (lsc_if_got_token() == false) {
			LISA_NLOGE("lsc auth faild !");
			return LSC_ERR;
		}
	}

	ret = lsc->conn->connect(lsc->auth_token);
	if (ret) {
		LISA_NLOGE("lsc connect faild(ret = %d)", ret);
		return LSC_ERR;
	}

	return LSC_OK;
}

int lsc_disconnect(void)
{
	lsc_t *lsc = g_lsc_obj;
	CHECK_COND_RETURN_VAL(lsc, LSC_INVALID_STATE, "lsc not create");

	LISA_NLOGI("lsc disconnect");

	int ret = lsc->conn->disconnect();
	if (ret) {
		LISA_NLOGE("lsc connect faild(ret = %d)", ret);
		return LSC_ERR;
	}

	lsc_clear_token();

	return LSC_OK;
}

int lsc_deinit(void)
{
	lsc_t *lsc = g_lsc_obj;
	CHECK_COND_RETURN_VAL(lsc, LSC_INVALID_STATE, "lsc not create");

	if (lsc->connected_sem) {
		lisa_semaphore_delete(lsc->connected_sem);
		lsc->connected_sem = NULL;
	}

	if (lsc->conn) {
		lsc_conn_destroy(lsc->conn);
		lsc->conn = NULL;
	}

	if (lsc->cb_list) {
		lisa_evt_publisher_destroy(lsc->cb_list);
		lsc->cb_list = NULL;
	}

	if (lsc->config) {
		clear_config(lsc->config);
		lsc->config = NULL;
	}

	if (lsc->reconnect_thread) {
		lisa_thread_delete(lsc->reconnect_thread);
	}
	lsc_clear_token();
	lisa_mem_free(lsc);
	sessions_core_deinit();

	g_lsc_obj = NULL;

	return LSC_OK;
}

static void _http_on_data(lisa_http_data_t *data)
{
	lsc_t *lsc = g_lsc_obj;
	cJSON *root = cJSON_Parse(data->buf);
	LISA_NLOGI("lsc music active, http on data:%s", data->buf);
	if (root) {
		cJSON *param = cJSON_GetObjectItem(root, "code");
		if (param && param->valueint == 200) {
			LISA_NLOGI("lsc music active success!!!!");
			lsc->music_active = true;
		} else {
			LISA_NLOGE("lsc music active faild!!!!");
			lsc->music_active = false;
		}
		cJSON_Delete(root);
	}
}

static void *_http_reg_headers(void)
{
	lsc_t *lsc = g_lsc_obj;
	return lsc->auth_header;
}

int lsc_music_active(void)
{
	int ret;
	lsc_t *lsc = g_lsc_obj;
	CHECK_COND_RETURN_VAL(lsc, LSC_INVALID_STATE, "lsc not create");

	CHECK_COND_RETURN_VAL(lsc->auth_token, LSC_INVALID_STATE, "lsc not auth");

	lisa_http_request_t req = {0};
	req.method = LISA_HTTP_POST;
	req.url = LSC_GET_MUSIC_ACTIVE_URL_FROM_SERVER(lsc->config->server_config);
	req.timeout = 3;
	req.on_data = _http_on_data;
	req.body = "{}";
	req.body_len = 2;
	req.headers = (uint8_t *)_http_music_req_url_headers;

	lisa_http_t *http = lisa_http_init(&req);
	CHECK_COND_GOTO(http, _err, "http init faild");

	ret = lisa_http_perform(http);
	CHECK_COND_GOTO(ret == LISA_HTTP_OK, _err, "http perform faild(ret=%d)", ret);

	lisa_http_cleanup(http);

	if (lsc->music_active == false) {
		return LSC_ERR;
	}

	return LSC_OK;
_err:
	if (http) {
		lisa_http_cleanup(http);
	}
	return LSC_ERR;
}

static void _http_music_req_url_data(lisa_http_data_t *data)
{
	LISA_NLOGI("http req music url %s", (char *)data->buf);

	cJSON *root = cJSON_Parse(data->buf);
	if (root) {
		cJSON *cj_data = cJSON_GetObjectItem(root, "data");
		if (cj_data && (cJSON_GetArraySize(cj_data) > 0)) {
			cJSON *item = cJSON_GetArrayItem(cj_data, 0);
			if (item) {
				cJSON *url = cJSON_GetObjectItem(item, "audiopath");
				if (url && url->valuestring) {
					strcpy(data->user, url->valuestring);
				}

				LISA_NLOGI("get music url %s", (char *)data->user);
				goto _destroy;
			}
		}
	}

	LISA_NLOGW("http req music url faild");

_destroy:
	if (root != NULL) {
		cJSON_Delete(root);
	}
}

// todo
char *http_req_headers_strings = NULL;
static void *_http_music_req_url_headers(void)
{
	lsc_t *lsc = g_lsc_obj;

	if (http_req_headers_strings) {
		lisa_mem_free(http_req_headers_strings);
		http_req_headers_strings = NULL;
	}

	http_req_headers_strings =
		lisa_mem_calloc(1, strlen(lsc->auth_header) + strlen("Content-Type: application/json") + 1);

	strcat(http_req_headers_strings, lsc->auth_header);
	strcat(http_req_headers_strings, "Content-Type: application/json");

	LISA_NLOGD("req headers:%s", http_req_headers_strings);

	return (void *)http_req_headers_strings;
}

int lsc_music_request_url(const char *music_item_id, char music_url[256])
{
	int ret;
	lsc_t *lsc = g_lsc_obj;
	CHECK_COND_RETURN_VAL(lsc, LSC_INVALID_STATE, "lsc not create");

	CHECK_COND_RETURN_VAL(lsc->auth_token, LSC_INVALID_STATE, "lsc not auth");

	LISA_NLOGI("http req music(id:%s) url", music_item_id);

	strcpy(music_url, "req faild");

	cJSON *jsonItem = cJSON_CreateObject();
	cJSON_AddStringToObject(jsonItem, "itemid", music_item_id);
	cJSON_AddStringToObject(jsonItem, "format", "128kmp3");

	char *req_body = cJSON_Print(jsonItem);

	lisa_http_request_t req = {0};
	req.method = LISA_HTTP_POST;
	req.url = LSC_GET_MUSIC_TRANLINK_URL_FROM_SERVER(lsc->config->server_config);
	req.timeout = 3;
	req.on_data = _http_music_req_url_data;
	req.body = req_body;
	req.body_len = strlen(req.body);
	req.headers = (uint8_t *)_http_music_req_url_headers;
	req.user = music_url;

	LISA_NLOGD("req body:%s len:%d", req_body, req.body_len);

	lisa_http_t *http = lisa_http_init(&req);
	CHECK_COND_GOTO(http, _err, "http init faild");

	ret = lisa_http_perform(http);
	CHECK_COND_GOTO(ret == LISA_HTTP_OK, _err, "http perform faild(ret=%d)", ret);

	lisa_http_cleanup(http);

	cJSON_free(req_body);
	cJSON_Delete(jsonItem);

	if (http_req_headers_strings) {
		lisa_mem_free(http_req_headers_strings);
		http_req_headers_strings = NULL;
	}

	if (strcmp(music_url, "req faild") == 0) {
		return LSC_ERR;
	}

	return LSC_OK;
_err:
	if (http) {
		lisa_http_cleanup(http);
	}

	if (req_body) {
		cJSON_free(req_body);
	}
	if (jsonItem) {
		cJSON_Delete(jsonItem);
	}
	if (http_req_headers_strings) {
		lisa_mem_free(http_req_headers_strings);
		http_req_headers_strings = NULL;
	}
	return LSC_ERR;
}

const char *get_lsc_jwt_token(void)
{
	lsc_t *lsc = g_lsc_obj;

	/* 检查lsc是否已初始化 */
	if (lsc == NULL) {
		LISA_NLOGW("LSC未初始化，无法获取JWT令牌");
		return NULL;
	}

	/* 检查是否已认证并获取令牌 */
	if (lsc->auth_token == NULL) {
		LISA_NLOGW("LSC未获取到JWT令牌");
		return NULL;
	}

	LISA_NLOGI("获取到LSC JWT令牌，长度: %d", strlen(lsc->auth_token));
	return lsc->auth_token;
}
