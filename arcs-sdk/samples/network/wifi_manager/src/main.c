#include "stdio.h"
#include "listen_wifi.h"
#include "stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_log.h"
#include "wifi_manager/wifi_manager.h"

#define WIFI_HOTSPOT_LIST_MAX_NUMBER  (10)

static void _main_wifi_stack_init_done_cb(void)
{
    LOGI("Wifi Stack Init Done");
}

static void wifi_mgr_connection_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    LOGI("mgr connection: %d", connection_info->status);
}

static void wifi_mgr_scan_done_cb(wifi_mgr_scan_info_t *aps_info, int ap_num, void *arg)
{
    int num;
    
    if (aps_info == NULL || ap_num <= 0) {
        LOGE("Scan done error");
        return;
    }

    LOGI("wifi manager scan done,update hotspot number:%d\n", ap_num);
    for (num = 0; num < ap_num; num++) {
       LOGI("ssid: %s rssi: %d", aps_info[num].ssid, aps_info[num].rssi);
    }
    LOGI("\n");
}

int main(int argc, char **argv)
{
    int ret;
    wifi_mgr_scan_info_t ap_info[WIFI_HOTSPOT_LIST_MAX_NUMBER];
    wifi_mgr_connection_status_t ret_status;

    wifi_mgr_sta_config_t cfg = {
        .ssid = "Xiaomi_listenai_2.4G",
        .pwd = "a12345678",
    };

    // wifi驱动初始化
    ls_wifi_pre_init(_main_wifi_stack_init_done_cb);
    
    // wifi管理初始化
    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();

    // 注册WiFi回调事件
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_cb, NULL);
    wifi_mgr_add_scan_done_cb(wifi_mgr_scan_done_cb, NULL);

    // 扫描附近的AP设备
    wifi_mgr_scan_ap(ap_info, WIFI_HOTSPOT_LIST_MAX_NUMBER, false);

    ret = wifi_mgr_sta_connect(&cfg, false);
    if (ret != 0) {
        LOGI("wifi connect failed");
        return -1;
    }

    while (1) {
        ret_status = wifi_mgr_sta_get_status();
        if (ret_status == WIFI_MGR_STA_CONNECTED) {
            wifi_mgr_sta_disconnect(false);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
