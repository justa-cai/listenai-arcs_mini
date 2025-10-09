#include "stdio.h"
#include "listen_wifi.h"
#include "stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_log.h"
#include "lisa_thread.h"
#include "wifi_manager/wifi_manager.h"
#include "lisa_http.h"
#include "wifi_manager/wifi_manager_ops.h"

#define SERVER_URL "http://192.168.32.152:8000"
#define HTTP_TEST_HEAD "Transfer-Encoding: chunked\r\n"

static void _main_wifi_stack_init_done_cb(void)
{
    LOGI("Wifi Stack Init Done");
}

static void wifi_mgr_connection_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    LOGI("mgr connection: %d", connection_info->status);
}

static int wifi_connect(wifi_mgr_sta_config_t *cfg)
{
    int ret;
    wifi_mgr_connection_status_t ret_status;

    // wifi驱动初始化
    ls_wifi_pre_init(_main_wifi_stack_init_done_cb);

    // wifi管理初始化
    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();

    // 注册WiFi回调事件
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_cb, NULL);
    
    ret = wifi_mgr_sta_connect(cfg, false);
    if (ret != 0) {
        LOGI("wifi connect failed");
        return -1;
    }

    while (1) {
        extern bool g_get_ip_success;
        if (g_get_ip_success) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    LOGI("wifi connect success");

    return 0;
}

static void http_data_cb(lisa_http_data_t *data)
{
    if (!data || !data->buf) {
        LOGE("http data is null");
        return;
    } else if (data->len <= 0) {
        LOGE("http data len is 0");
        return;
    }
    else {
        LOGI("\nhttp response : %s\n", data->buf);
    }
}

static void *http_request_heads(void)
{
    return HTTP_TEST_HEAD;
}

int main(int argc, char **argv)
{
    int ret;
    lisa_http_t *http = NULL;

    wifi_mgr_sta_config_t cfg = {
        .ssid = "Xiaomi_listenai_2.4G",
        .pwd = "a12345678",
    };

    ret = wifi_connect(&cfg);
    if (ret != 0) {
        return -1;
    }

    lisa_http_request_t req;

    memset(&req, 0, sizeof(lisa_http_request_t));
	req.method = LISA_HTTP_GET;
	req.url = SERVER_URL;
	req.timeout = 10;
	req.on_data = http_data_cb;
	req.body = NULL;
	req.body_len = 0;
	req.headers = NULL;

    http = lisa_http_init(&req);
	if (!http) {
		LOGE("[http] init err\n");
		goto ERR;
	}

    if (lisa_http_perform(http) != 0) {
        LOGE("http get request info err..\n");
        goto ERR;
    }

    if (http) {
        lisa_http_cleanup(http);
        http = NULL;
    }

    memset(&req, 0, sizeof(lisa_http_request_t));
    req.method = LISA_HTTP_POST;
	req.url = SERVER_URL"/foobar";
	req.timeout = 10;
	req.on_data = http_data_cb;
	req.body = "foobar";
	req.body_len = strlen(req.body);
	req.headers = NULL;

    http = lisa_http_init(&req);
	if (!http) {
		LOGE("[http] init err\n");
		goto ERR;
	}

    if (lisa_http_perform(http) != 0) {
        LOGE("http get request info err..\n");
        goto ERR;
    }

    if (http) {
        lisa_http_cleanup(http);
        http = NULL;
    }

    memset(&req, 0, sizeof(lisa_http_request_t));

    const char *content[] = {
		"foobar",
		"chunked",
		"last"
	};
	char tmp[64];
	int i, pos = 0;

	for (i = 0; i < ARRAY_SIZE(content); i++) {
		pos += snprintf(tmp + pos, sizeof(tmp) - pos,
				"%x\r\n%s\r\n",
				(unsigned int)strlen(content[i]),
				content[i]);
	}

	pos += snprintf(tmp + pos, sizeof(tmp) - pos, "0\r\n\r\n");

    req.method = LISA_HTTP_POST;
	req.url = SERVER_URL"/chunked-test";
	req.timeout = 10;
	req.on_data = http_data_cb;

    req.body = tmp;
	req.body_len = strlen(req.body);
	req.headers = (void *)http_request_heads;

    http = lisa_http_init(&req);
	if (!http) {
		LOGE("[http] init err\n");
		goto ERR;
	}

    if (lisa_http_perform(http) != 0) {
        LOGE("http get request info err..\n");
        goto ERR;
    }

    if (http) {
        lisa_http_cleanup(http);
        http = NULL;
    }

ERR:
    if (http) {
        lisa_http_cleanup(http);
        http = NULL;
    }

    return 0;
}
