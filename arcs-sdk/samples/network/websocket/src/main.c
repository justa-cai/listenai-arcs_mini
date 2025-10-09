#include "stdio.h"
#include "listen_wifi.h"
#include "stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_log.h"
#include "lisa_thread.h"
#include "wifi_manager/wifi_manager.h"
#include "lisa_websocket.h"

#define WS_TEST_SCHEME  "ws"
#define WS_TEST_HOST    "192.168.32.152"
#define WS_TEST_PORT    "9001"
#define WS_TEST_PATH    "/"
#define WS_SEND_RETRY_DELAY (10000)
#define WS_SEND_RETRY_SLEEP_TIME (100)

#define WS_TEST_CONTENT "lisa_test"
#define WS_TEST_MAX_COUNT  (10)

static lisa_ws_event_e g_event = LISA_WS_ON_DISCONNECTED;

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

static void get_ws_data_cb(lisa_ws_data_t *data)
{
    LOGI("ws recv data: %s", data->buf);
}

static void get_ws_event_cb(lisa_ws_event_t *event)
{
    g_event = event->what;
	if (event->what == LISA_WS_ON_CONNECTED) {
        LOGI("lisa ws connect");
	} else if (event->what == LISA_WS_ON_DISCONNECTED) {
		LOGI("lisa ws disconnect");
	} else {
		LOGI("event %d", event->what);
	}
}

int main(int argc, char **argv)
{
    int ret;

    wifi_mgr_sta_config_t cfg = {
        .ssid = "Xiaomi_listenai_2.4G",
        .pwd = "a12345678",
    };

    ret = wifi_connect(&cfg);
    if (ret != 0) {
        return -1;
    }

    g_event = LISA_WS_ON_DISCONNECTED;
    lisa_ws_request_t lisa_ws_req;
    lisa_ws_req.scheme = WS_TEST_SCHEME;
	lisa_ws_req.host = WS_TEST_HOST;
	lisa_ws_req.port = WS_TEST_PORT;
	lisa_ws_req.path = WS_TEST_PATH;
	lisa_ws_req.timeout = WS_SEND_RETRY_DELAY;
	lisa_ws_req.on_data = get_ws_data_cb;
	lisa_ws_req.on_event = get_ws_event_cb;
	lisa_ws_req.user = NULL;
    lisa_ws_req.extra_header = NULL;

    lisa_ws_t* lisa_ws_ins = lisa_ws_init(&lisa_ws_req);
    if (lisa_ws_ins == NULL) {
        LOGE("lisa ws init failed");
        return -1;
    }

    if (lisa_ws_connect(lisa_ws_ins) != 0) {
        LOGE("lisa evs websocket connect error");
        ret = -1;
        goto ERR;
    }

    int check_conn_count = 0;
    while (g_event == LISA_WS_ON_DISCONNECTED) {
        lisa_thread_mdelay(WS_SEND_RETRY_SLEEP_TIME);
        check_conn_count++;
        if (check_conn_count >= (WS_SEND_RETRY_DELAY / WS_SEND_RETRY_SLEEP_TIME)) {
            LOGE("lisa ws connect timeout");
            goto ERR;
        }
    }

    int ws_test_count = 0;
    while (1) {
        lisa_ws_send_text(lisa_ws_ins, WS_TEST_CONTENT);
        lisa_thread_mdelay(1000);
        ws_test_count++;
        if (ws_test_count >= WS_TEST_MAX_COUNT) {
            break;
        }
    }

    LOGI("ws send and recv test success");
    ret = 0;
ERR:
    if (lisa_ws_ins != NULL) {
        lisa_ws_disconnect(lisa_ws_ins);
        lisa_ws_cleanup(lisa_ws_ins);
        lisa_ws_ins = NULL;
    }
    return ret;
}
