#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#define TAG "model_wifi"

#include "lisa_ui_invoke.h"
#include "lisa_ui_log.h"

#include "model_wifi.h"

#ifdef LISA_UI_PLATFORM_ARCS
#include "wifi_manager/wifi_manager.h"
#include "voice_msg.h"
#endif

struct model_wifi_context {
    uint32_t inited: 1;
    uint32_t scanning: 1;

    const struct model_wifi_cb *cbs;
    void *arg;

    model_wifi_status_t status;
    model_wifi_scan_info_t scan_list[WIFI_MAX_SCAN_APS];
    int scan_count;
};

static struct model_wifi_context model_wifi_ctx = {
    .inited = 0,
    .scanning = 0,
    .status = MODEL_WIFI_STATUS_DISCONNECTED,
    .scan_count = 0,
};

#ifdef LISA_UI_PLATFORM_ARCS

// WiFi Manager 到 Model 的加密模式转换
static model_wifi_encryption_mode_t convert_encryption_mode(wifi_mgr_wifi_encryption_mode_t mode)
{
    switch (mode) {
        case WIFI_MGR_WIFI_AUTH_AUTO:
            return MODEL_WIFI_AUTH_AUTO;
        case WIFI_MGR_WIFI_AUTH_OPEN:
            return MODEL_WIFI_AUTH_OPEN;
        case WIFI_MGR_WIFI_AUTH_WEP:
            return MODEL_WIFI_AUTH_WEP;
        case WIFI_MGR_WIFI_AUTH_WPA_PSK:
            return MODEL_WIFI_AUTH_WPA_PSK;
        case WIFI_MGR_WIFI_AUTH_WPA2_PSK:
            return MODEL_WIFI_AUTH_WPA2_PSK;
        case WIFI_MGR_WIFI_AUTH_WPA_WPA2_PSK:
            return MODEL_WIFI_AUTH_WPA_WPA2_PSK;
        case WIFI_MGR_WIFI_AUTH_WPA2_ENTERPRISE:
            return MODEL_WIFI_AUTH_WPA2_ENTERPRISE;
        case WIFI_MGR_WIFI_AUTH_WPA3_PSK:
            return MODEL_WIFI_AUTH_WPA3_PSK;
        case WIFI_MGR_WIFI_AUTH_WPA2_WPA3_PSK:
            return MODEL_WIFI_AUTH_WPA2_WPA3_PSK;
        default:
            return MODEL_WIFI_AUTH_UNKNOWN;
    }
}

// Model 到 WiFi Manager 的加密模式转换
static wifi_mgr_wifi_encryption_mode_t convert_to_wifi_mgr_encryption_mode(model_wifi_encryption_mode_t mode)
{
    switch (mode) {
        case MODEL_WIFI_AUTH_AUTO:
            return WIFI_MGR_WIFI_AUTH_AUTO;
        case MODEL_WIFI_AUTH_OPEN:
            return WIFI_MGR_WIFI_AUTH_OPEN;
        case MODEL_WIFI_AUTH_WEP:
            return WIFI_MGR_WIFI_AUTH_WEP;
        case MODEL_WIFI_AUTH_WPA_PSK:
            return WIFI_MGR_WIFI_AUTH_WPA_PSK;
        case MODEL_WIFI_AUTH_WPA2_PSK:
            return WIFI_MGR_WIFI_AUTH_WPA2_PSK;
        case MODEL_WIFI_AUTH_WPA_WPA2_PSK:
            return WIFI_MGR_WIFI_AUTH_WPA_WPA2_PSK;
        case MODEL_WIFI_AUTH_WPA2_ENTERPRISE:
            return WIFI_MGR_WIFI_AUTH_WPA2_ENTERPRISE;
        case MODEL_WIFI_AUTH_WPA3_PSK:
            return WIFI_MGR_WIFI_AUTH_WPA3_PSK;
        case MODEL_WIFI_AUTH_WPA2_WPA3_PSK:
            return WIFI_MGR_WIFI_AUTH_WPA2_WPA3_PSK;
        default:
            return WIFI_MGR_WIFI_AUTH_UNKNOWN;
    }
}

// WiFi Manager 到 Model 的状态转换
static model_wifi_status_t convert_wifi_status(wifi_mgr_connection_status_t status)
{
    switch (status) {
        case WIFI_MGR_STA_CONNECTED:
            return MODEL_WIFI_STATUS_CONNECTED;
        case WIFI_MGR_STA_CONNECTING:
            return MODEL_WIFI_STATUS_CONNECTING;
        case WIFI_MGR_STA_DISCONNECTED:
            return MODEL_WIFI_STATUS_DISCONNECTED;
        default:
            return MODEL_WIFI_STATUS_UNKNOWN;
    }
}

// WiFi 连接状态回调
static void wifi_connection_event_handler(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    if (!connection_info) {
        return;
    }
    LISA_UI_LOGI("wifi connecting info, ssid: %s, reason: %d", connection_info->sta_info->ssid,
                 connection_info->reason);
    wifi_mgr_connection_status_t status = connection_info->status;
    int reason = connection_info->reason;

    LISA_UI_INVOKE_UI_ARG_BASE(status, {
        model_wifi_ctx.status = convert_wifi_status(_invoke_status);

        if (model_wifi_ctx.cbs) {
            switch (_invoke_status) {
                case WIFI_MGR_STA_CONNECTED:
                    if (model_wifi_ctx.cbs->on_connected) {
                        model_wifi_sta_config_t sta_info;
                        if (model_wifi_get_connected_info(&sta_info) == 0) {
                            model_wifi_ctx.cbs->on_connected(&sta_info, model_wifi_ctx.arg);
                        }
                    }
                    break;

                case WIFI_MGR_STA_CONNECTING:
                    if (model_wifi_ctx.cbs->on_connecting) {
                        model_wifi_ctx.cbs->on_connecting(model_wifi_ctx.arg);
                    }
                    break;

                case WIFI_MGR_STA_DISCONNECTED:
                    if (model_wifi_ctx.cbs->on_disconnected) {
                        model_wifi_ctx.cbs->on_disconnected(0, model_wifi_ctx.arg);
                    }
                    break;

                default:
                    break;
            }
        }
    });
}

// WiFi 扫描完成回调
static void wifi_scan_done_handler(wifi_mgr_scan_info_t *aps_info, int ap_num, void *arg)
{
    if (ap_num > 0 && aps_info) {
        // 计算需要拷贝的AP数量
        int copy_count = ap_num > WIFI_MAX_SCAN_APS ? WIFI_MAX_SCAN_APS : ap_num;
        uint32_t data_len = sizeof(wifi_mgr_scan_info_t) * copy_count;

        LISA_UI_INVOKE_UI_ARG_PTR(aps_info, data_len, {
            wifi_mgr_scan_info_t *scan_result = (wifi_mgr_scan_info_t *)_invoke_aps_info;
            int count = _invoke_len / sizeof(wifi_mgr_scan_info_t);

            model_wifi_ctx.scanning = 0;
            model_wifi_ctx.scan_count = count;

            // 转换扫描结果到 model 层数据结构
            for (int i = 0; i < count; i++) {
                strncpy(model_wifi_ctx.scan_list[i].ssid, scan_result[i].ssid, WIFI_SSID_MAX_LEN - 1);
                model_wifi_ctx.scan_list[i].ssid[WIFI_SSID_MAX_LEN - 1] = '\0';

                strncpy(model_wifi_ctx.scan_list[i].bssid, scan_result[i].bssid, WIFI_BSSID_MAX_LEN - 1);
                model_wifi_ctx.scan_list[i].bssid[WIFI_BSSID_MAX_LEN - 1] = '\0';

                model_wifi_ctx.scan_list[i].channel = scan_result[i].channel;
                model_wifi_ctx.scan_list[i].rssi = scan_result[i].rssi;
                model_wifi_ctx.scan_list[i].encryption_mode = convert_encryption_mode(scan_result[i].encryption_mode);
            }

            if (model_wifi_ctx.cbs && model_wifi_ctx.cbs->on_scan_done) {
                model_wifi_ctx.cbs->on_scan_done(model_wifi_ctx.scan_list, count, model_wifi_ctx.arg);
            }
        });
    } else {
        // 扫描失败
        int num = ap_num;
        LISA_UI_INVOKE_UI_ARG_BASE(num, {
            model_wifi_ctx.scanning = 0;
            model_wifi_ctx.scan_count = 0;

            if (model_wifi_ctx.cbs && model_wifi_ctx.cbs->on_scan_failed) {
                model_wifi_ctx.cbs->on_scan_failed(_invoke_num, model_wifi_ctx.arg);
            }
        });
    }
}

static void wifi_disconnected_msg_handle(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LISA_UI_INVOKE_UI_ARG_NONE({
        model_wifi_ctx.status = MODEL_WIFI_STATUS_DISCONNECTED;
    });
}

static void wifi_ip_got_msg_handle(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LISA_UI_INVOKE_UI_ARG_NONE({
        model_wifi_ctx.status = MODEL_WIFI_STATUS_CONNECTED;
    });
}

#endif // LISA_UI_PLATFORM_ARCS

int model_wifi_init(void)
{
    if (model_wifi_ctx.inited) {
        return 0;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    int ret;

    ret = wifi_mgr_sta_add_connection_cb(wifi_connection_event_handler, NULL);
    if (ret != 0) {
        LISA_UI_LOGE("Failed to add connection callback: %d", ret);
        return ret;
    }

    ret = wifi_mgr_add_scan_done_cb(wifi_scan_done_handler, NULL);
    if (ret != 0) {
        LISA_UI_LOGE("Failed to add scan done callback: %d", ret);
        wifi_mgr_sta_remove_connection_cb(wifi_connection_event_handler);
        return ret;
    }

    wifi_mgr_connection_status_t status = wifi_mgr_sta_get_status();
    model_wifi_ctx.status = convert_wifi_status(status);

    LISA_UI_LOGD("WiFi model initialized, status: %d", model_wifi_ctx.status);

    voice_msg_sub(VOICE_MSG_WIFI_DISCONNECTED, wifi_disconnected_msg_handle, NULL);
    voice_msg_sub(VOICE_MSG_WIFI_IP_GOT, wifi_ip_got_msg_handle, NULL);
#endif

    model_wifi_ctx.inited = 1;

    return 0;
}

int model_wifi_deinit(void)
{
    if (!model_wifi_ctx.inited) {
        return 0;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    wifi_mgr_sta_remove_connection_cb(wifi_connection_event_handler);
    wifi_mgr_remove_scan_done_cb(wifi_scan_done_handler);
    voice_msg_unsub(VOICE_MSG_WIFI_DISCONNECTED, wifi_disconnected_msg_handle);
#endif

    model_wifi_ctx.inited = 0;
    model_wifi_ctx.cbs = NULL;
    model_wifi_ctx.arg = NULL;

    return 0;
}

int model_wifi_cb_register(const struct model_wifi_cb *cb, void *arg)
{
    model_wifi_ctx.cbs = cb;
    model_wifi_ctx.arg = arg;
    return 0;
}

int model_wifi_cb_unregister(const struct model_wifi_cb *cb)
{
    model_wifi_ctx.cbs = NULL;
    model_wifi_ctx.arg = NULL;
    return 0;
}

int model_wifi_scan_start(void)
{
    if (!model_wifi_ctx.inited) {
        LISA_UI_LOGE("WiFi model not initialized");
        return -1;
    }

    // if (model_wifi_ctx.scanning) {
    //     LISA_UI_LOGW("WiFi scan already in progress");
    //     return -2;
    // }

#ifdef LISA_UI_PLATFORM_ARCS
    model_wifi_ctx.scanning = 1;

    LISA_UI_INVOKE_BN_ARG_NONE({
        wifi_mgr_scan_info_t ap_info[WIFI_MAX_SCAN_APS];
        int ret = wifi_mgr_scan_ap(ap_info, WIFI_MAX_SCAN_APS, false);

        // 扫描结果会通过回调返回
        if (ret < 0) {
            LISA_UI_LOGE("WiFi scan failed: %d", ret);
        }
    });
#else
    LISA_UI_LOGW("WiFi scan not supported on this platform");
    return -3;
#endif

    return 0;
}

model_wifi_status_t model_wifi_get_status(void)
{
    return model_wifi_ctx.status;
}

int model_wifi_connect(const char *ssid, const char *pwd, const char *bssid)
{
    if (!model_wifi_ctx.inited) {
        LISA_UI_LOGE("WiFi model not initialized");
        return -1;
    }

    if (!ssid) {
        LISA_UI_LOGE("SSID cannot be NULL");
        return -2;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    wifi_mgr_sta_config_t sta_config;
    memset(&sta_config, 0, sizeof(sta_config));

    strncpy(sta_config.ssid, ssid, sizeof(sta_config.ssid) - 1);
    sta_config.ssid[sizeof(sta_config.ssid) - 1] = '\0';

    if (pwd) {
        strncpy(sta_config.pwd, pwd, sizeof(sta_config.pwd) - 1);
        sta_config.pwd[sizeof(sta_config.pwd) - 1] = '\0';
    }

    if (bssid) {
        strncpy(sta_config.bssid, bssid, sizeof(sta_config.bssid) - 1);
        sta_config.bssid[sizeof(sta_config.bssid) - 1] = '\0';
    }

    sta_config.encryption_mode = WIFI_MGR_WIFI_AUTH_AUTO;

    wifi_mgr_sta_config_t *config = &sta_config;
    LISA_UI_INVOKE_BN_ARG_PTR(config, sizeof(sta_config), {
        wifi_mgr_sta_config_t *cfg = (wifi_mgr_sta_config_t *)_invoke_config;
        int ret = wifi_mgr_sta_connect(cfg, false);

        if (ret != 0) {
            LISA_UI_INVOKE_UI_ARG_NONE({
                if (model_wifi_ctx.cbs && model_wifi_ctx.cbs->on_connection_failed) {
                    model_wifi_ctx.cbs->on_connection_failed(-1, model_wifi_ctx.arg);
                }
            });
        }
    });
#else
    LISA_UI_LOGW("WiFi connect not supported on this platform");
    return -3;
#endif

    return 0;
}

int model_wifi_disconnect(void)
{
    if (!model_wifi_ctx.inited) {
        LISA_UI_LOGE("WiFi model not initialized");
        return -1;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    LISA_UI_INVOKE_BN_ARG_NONE({
        int ret = wifi_mgr_sta_disconnect(false);
        if (ret != 0) {
            LISA_UI_LOGE("WiFi disconnect failed: %d", ret);
        } else {
            LISA_UI_LOGD("WiFi disconnected");
        }
    });
#else
    LISA_UI_LOGW("WiFi disconnect not supported on this platform");
    return -2;
#endif

    return 0;
}

int model_wifi_get_connected_info(model_wifi_sta_config_t *sta_info)
{
    if (!model_wifi_ctx.inited) {
        LISA_UI_LOGE("WiFi model not initialized");
        return -1;
    }

    if (!sta_info) {
        LISA_UI_LOGE("sta_info cannot be NULL");
        return -2;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    wifi_mgr_sta_config_t wifi_sta_info;
    int ret = wifi_mgr_sta_get_connected_info(&wifi_sta_info);

    if (ret != 0) {
        LISA_UI_LOGE("Failed to get WiFi connected info: %d", ret);
        return ret;
    }

    // 转换数据结构
    strncpy(sta_info->ssid, wifi_sta_info.ssid, WIFI_SSID_MAX_LEN - 1);
    sta_info->ssid[WIFI_SSID_MAX_LEN - 1] = '\0';

    strncpy(sta_info->bssid, wifi_sta_info.bssid, WIFI_BSSID_MAX_LEN - 1);
    sta_info->bssid[WIFI_BSSID_MAX_LEN - 1] = '\0';

    strncpy(sta_info->pwd, wifi_sta_info.pwd, WIFI_PWD_MAX_LEN - 1);
    sta_info->pwd[WIFI_PWD_MAX_LEN - 1] = '\0';

    sta_info->channel = wifi_sta_info.channel;
    sta_info->rssi = wifi_sta_info.rssi;
    sta_info->encryption_mode = convert_encryption_mode(wifi_sta_info.encryption_mode);

    return 0;
#else
    LISA_UI_LOGW("Get WiFi connected info not supported on this platform");
    return -3;
#endif
}
