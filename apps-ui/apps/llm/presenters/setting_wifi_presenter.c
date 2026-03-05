/**
 * @file setting_wifi_presenter.c
 * @brief WiFi settings presenter - handles data logic only
 */

#define TAG "setting_wifi_presenter"

#include <stdlib.h>
#include <string.h>
#include "lisa_ui_nav_scr.h"
#include "lisa_ui.h"
#include "lisa_ui_nav_scr_ids.h"
#include "setting_wifi_view.h"
#include "model_wifi.h"
#include "lisa_ui_toast.h"

struct setting_wifi_nav_scr_data {
    lv_obj_t *view;
    bool wifi_enabled;
    bool model_inited;
    lv_timer_t *scan_timer;  /* WiFi扫描定时器 */
};

static struct setting_wifi_nav_scr_data *g_scr_data = NULL;

/**
 * @brief WiFi 定时扫描回调
 */
static void wifi_scan_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!g_scr_data || !g_scr_data->model_inited) {
        return;
    }

    LISA_UI_LOGD("Timer triggered WiFi scan");
    int ret = model_wifi_scan_start();
    if (ret != 0) {
        LISA_UI_LOGE("Timer: Failed to start WiFi scan: %d", ret);
    }
}

/**
 * @brief WiFi 连接成功回调
 */
static void on_wifi_connected(model_wifi_sta_config_t *sta_info, void *arg)
{
    (void)arg;
    if (sta_info) {
        LISA_UI_LOGD("WiFi connected to: %s", sta_info->ssid);

        /* 重新扫描WiFi列表以更新连接状态 */
        if (g_scr_data && g_scr_data->model_inited) {
            int ret = model_wifi_scan_start();
            if (ret != 0) {
                LISA_UI_LOGE("Failed to start WiFi scan after connection: %d", ret);
            }
        }
    }
}

/**
 * @brief WiFi 断开连接回调
 */
static void on_wifi_disconnected(int reason, void *arg)
{
    (void)arg;
    LISA_UI_LOGD("WiFi disconnected, reason: %d", reason);

    /* 重新扫描WiFi列表以更新连接状态 */
    if (g_scr_data && g_scr_data->model_inited) {
        int ret = model_wifi_scan_start();
        if (ret != 0) {
            LISA_UI_LOGE("Failed to start WiFi scan after disconnection: %d", ret);
        }
    }
}

/**
 * @brief WiFi 正在连接回调
 */
static void on_wifi_connecting(void *arg)
{
    (void)arg;
    LISA_UI_LOGD("WiFi connecting...");
    // TODO: 更新UI显示连接中状态
}

/**
 * @brief WiFi 连接失败回调
 */
static void on_wifi_connection_failed(int reason, void *arg)
{
    (void)arg;
    LISA_UI_LOGE("WiFi connection failed, reason: %d", reason);
    lisa_ui_toast_show("连接失败");
}

/**
 * @brief RSSI 比较函数 - 用于排序
 * 排序规则:
 * 1. 已连接的WiFi排在最前面
 * 2. 其余WiFi按信号强度从高到低排序
 */
static int rssi_compare(const void *a, const void *b)
{
    const lisa_ui_wifi_ap_info_t *ap_a = (const lisa_ui_wifi_ap_info_t *)a;
    const lisa_ui_wifi_ap_info_t *ap_b = (const lisa_ui_wifi_ap_info_t *)b;

    /* 如果a已连接，b未连接，a排在前面 */
    if (ap_a->is_connected && !ap_b->is_connected) {
        return -1;
    }
    /* 如果b已连接，a未连接，b排在前面 */
    if (!ap_a->is_connected && ap_b->is_connected) {
        return 1;
    }

    /* 两者连接状态相同，按RSSI值越大，信号越强，应该排在前面 */
    return ap_b->rssi - ap_a->rssi;
}

/**
 * @brief WiFi 扫描完成回调
 */
static void on_wifi_scan_done(model_wifi_scan_info_t *aps_info, int ap_num, void *arg)
{
    (void)arg;
    LISA_UI_LOGD("WiFi scan done, found %d APs", ap_num);

    if (!g_scr_data || !g_scr_data->view) {
        return;
    }

    if (ap_num <= 0) {
        /* 没有扫描到 WiFi */
        lisa_ui_setting_wifi_view_clear_list(g_scr_data->view);
        return;
    }

    /* 获取当前连接的WiFi信息 */
    model_wifi_sta_config_t connected_sta;
    bool has_connected = false;
    if (model_wifi_get_connected_info(&connected_sta) == 0) {
        has_connected = true;
        LISA_UI_LOGD("Currently connected to: %s", connected_sta.ssid);
    }

    /* 转换为 view 层的数据结构 */
    lisa_ui_wifi_ap_info_t *ap_list = lisa_ui_malloc(sizeof(lisa_ui_wifi_ap_info_t) * ap_num);
    if (!ap_list) {
        LISA_UI_LOGE("Failed to allocate memory for AP list");
        return;
    }

    int unique_count = 0;
    for (int i = 0; i < ap_num; i++) {
        /* 过滤空SSID */
        if (aps_info[i].ssid[0] == '\0' || strlen(aps_info[i].ssid) == 0) {
            LISA_UI_LOGD("Skipping empty SSID at index %d", i);
            continue;
        }

        /* 检查是否已存在相同的SSID (去重) */
        bool is_duplicate = false;
        for (int j = 0; j < unique_count; j++) {
            if (strcmp(ap_list[j].ssid, aps_info[i].ssid) == 0) {
                is_duplicate = true;
                /* 如果发现重复，保留信号更强的 */
                if (aps_info[i].rssi > ap_list[j].rssi) {
                    LISA_UI_LOGD("Found duplicate SSID '%s', updating with stronger signal (%d > %d)",
                                 aps_info[i].ssid, aps_info[i].rssi, ap_list[j].rssi);
                    ap_list[j].rssi = aps_info[i].rssi;
                    ap_list[j].channel = aps_info[i].channel;
                }
                break;
            }
        }

        if (is_duplicate) {
            continue;
        }

        /* 添加到列表 */
        strncpy(ap_list[unique_count].ssid, aps_info[i].ssid, sizeof(ap_list[unique_count].ssid) - 1);
        ap_list[unique_count].ssid[sizeof(ap_list[unique_count].ssid) - 1] = '\0';
        ap_list[unique_count].rssi = aps_info[i].rssi;
        ap_list[unique_count].channel = aps_info[i].channel;

        /* 检查是否为当前连接的WiFi */
        ap_list[unique_count].is_connected = has_connected &&
                                              (strcmp(ap_list[unique_count].ssid, connected_sta.ssid) == 0);

        unique_count++;
    }

    /* 如果过滤后没有WiFi */
    if (unique_count == 0) {
        LISA_UI_LOGD("No valid WiFi APs after filtering");
        lisa_ui_free(ap_list);
        lisa_ui_setting_wifi_view_clear_list(g_scr_data->view);
        return;
    }

    LISA_UI_LOGD("Filtered %d -> %d unique APs", ap_num, unique_count);

    /* 按照信号强度排序 (RSSI从高到低) */
    qsort(ap_list, unique_count, sizeof(lisa_ui_wifi_ap_info_t), rssi_compare);

    for (int i = 0; i < unique_count; i++) {
        LISA_UI_LOGD("AP[%d]: SSID=%s, RSSI=%d, Channel=%d, Connected=%d",
                     i, ap_list[i].ssid, ap_list[i].rssi, ap_list[i].channel, ap_list[i].is_connected);
    }

    /* 更新 UI 显示 WiFi 列表 */
    lisa_ui_setting_wifi_view_update_list(g_scr_data->view, ap_list, unique_count);

    lisa_ui_free(ap_list);
}

/**
 * @brief WiFi 扫描失败回调
 */
static void on_wifi_scan_failed(int reason, void *arg)
{
    (void)arg;
    LISA_UI_LOGE("WiFi scan failed, reason: %d", reason);
    // TODO: 更新UI显示扫描失败
}

// WiFi model 回调结构
static const struct model_wifi_cb wifi_callbacks = {
    .on_connected = on_wifi_connected,
    .on_disconnected = on_wifi_disconnected,
    .on_connecting = on_wifi_connecting,
    .on_connection_failed = on_wifi_connection_failed,
    .on_scan_done = on_wifi_scan_done,
    .on_scan_failed = on_wifi_scan_failed,
};

/**
 * @brief 密码输入确认回调
 */
static void on_password_confirm(const char *ssid, const char *password, void *user_data)
{
    (void)user_data;
    LISA_UI_LOGD("Password confirmed for SSID: %s", ssid);

    if (!g_scr_data || !g_scr_data->view) {
        return;
    }

    if (password == NULL || strlen(password) < 8) {
        lisa_ui_toast_show("密码长度需大于8个字符");
        return;
    }

    /* 隐藏密码输入界面 */
    lisa_ui_setting_wifi_view_hide_password_input(g_scr_data->view);

    /* 尝试连接WiFi */
    LISA_UI_LOGI("Attempting to connect to: %s", ssid);
    int ret = model_wifi_connect(ssid, password, NULL);
    if (ret != 0) {
        LISA_UI_LOGE("Failed to start WiFi connection: %d", ret);
    }
}

/**
 * @brief 密码输入取消回调
 */
static void on_password_cancel(void *user_data)
{
    (void)user_data;
    LISA_UI_LOGD("Password input cancelled");
}

/**
 * @brief WiFi 项点击回调
 */
static void on_wifi_item_clicked(const char *ssid, void *user_data)
{
    (void)user_data;
    LISA_UI_LOGD("WiFi item clicked: %s", ssid);

    if (!g_scr_data || !g_scr_data->view) {
        return;
    }

    /* 检查是否为当前已连接的WiFi */
    model_wifi_sta_config_t connected_sta;
    if (model_wifi_get_connected_info(&connected_sta) == 0) {
        if (strcmp(connected_sta.ssid, ssid) == 0) {
            LISA_UI_LOGD("Clicked WiFi is already connected, ignoring");
            return;
        }
    }

    /* 显示密码输入界面 */
    lisa_ui_setting_wifi_view_show_password_input(g_scr_data->view, ssid,
                                                   on_password_confirm,
                                                   on_password_cancel,
                                                   g_scr_data);
}

/**
 * @brief 返回按钮回调 - 处理返回到设置主页面的导航
 * 这是presenter层的业务逻辑
 */
static void setting_wifi_back_btn_cb(void *user_data)
{
    (void)user_data;
    LISA_UI_LOGD("WiFi settings back button clicked, returning to settings");
    lisa_ui_nav_scr_nav_back();
}

static int setting_wifi_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    LISA_UI_LOGD("WiFi settings nav scr open");

    struct setting_wifi_nav_scr_data *scr_data = lisa_ui_malloc(sizeof(struct setting_wifi_nav_scr_data));
    if (!scr_data) {
        return -1;
    }

    memset(scr_data, 0, sizeof(struct setting_wifi_nav_scr_data));

    scr_data->wifi_enabled = true;  /* 默认开启自动扫描 */
    scr_data->model_inited = false;
    scr_data->scan_timer = NULL;
    g_scr_data = scr_data;

    // 初始化 WiFi model
    int ret = model_wifi_init();
    if (ret != 0) {
        LISA_UI_LOGE("Failed to initialize WiFi model: %d", ret);
    } else {
        scr_data->model_inited = true;

        // 注册 WiFi 回调
        ret = model_wifi_cb_register(&wifi_callbacks, scr_data);
        if (ret != 0) {
            LISA_UI_LOGE("Failed to register WiFi callbacks: %d", ret);
        }
    }

    // Create WiFi view (UI layout handled by view)
    scr_data->view = lisa_ui_setting_wifi_view_create(lv_scr_act());
    if (!scr_data->view) {
        LISA_UI_LOGE("Failed to create WiFi view");
        if (scr_data->model_inited) {
            model_wifi_cb_unregister(&wifi_callbacks);
        }
        lisa_ui_free(scr_data);
        g_scr_data = NULL;
        return -1;
    }

    // Attach data logic to auto scan switch
    // lv_obj_t *wifi_switch = lisa_ui_setting_wifi_view_get_switch(scr_data->view);
    // if (wifi_switch) {
    //     // 设置开关初始状态为开启
    //     // lv_obj_add_state(wifi_switch, LV_STATE_CHECKED);
    //     lv_obj_add_event_cb(wifi_switch, wifi_switch_cb, LV_EVENT_VALUE_CHANGED, scr_data);
    // }

    /* 注册返回按钮回调 */
    lisa_ui_setting_wifi_view_set_back_cb(scr_data->view, setting_wifi_back_btn_cb, NULL);

    /* 注册 WiFi 项点击回调 */
    lisa_ui_setting_wifi_view_set_item_cb(scr_data->view, on_wifi_item_clicked, scr_data);

    /* 立即扫描一次 */
    model_wifi_scan_start();

    scr_data->scan_timer = lv_timer_create(wifi_scan_timer_cb, 1000 * 20, NULL);
    lv_timer_resume(g_scr_data->scan_timer);
    if (scr_data->scan_timer) {
        LISA_UI_LOGD("WiFi scan timer created (10s interval)");
    } else {
        LISA_UI_LOGE("Failed to create WiFi scan timer");
    }

    *data = scr_data;

    return 0;
}

static int setting_wifi_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("WiFi settings nav scr show");
    struct setting_wifi_nav_scr_data *scr_data = (struct setting_wifi_nav_scr_data *)data;
    if (!scr_data || !scr_data->view) {
        LISA_UI_LOGE("WiFi view is NULL");
        return -1;
    }

    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int setting_wifi_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct setting_wifi_nav_scr_data *scr_data = (struct setting_wifi_nav_scr_data *)data;
    if (scr_data) {
        /* 删除定时器 */
        if (scr_data->scan_timer) {
            lv_timer_del(scr_data->scan_timer);
            scr_data->scan_timer = NULL;
            LISA_UI_LOGD("WiFi scan timer deleted");
        }

        // 注销 WiFi 回调
        if (scr_data->model_inited) {
            model_wifi_cb_unregister(&wifi_callbacks);
        }

        if (scr_data->view) {
            lv_obj_del(scr_data->view);
        }

        lisa_ui_free(scr_data);
        g_scr_data = NULL;
    }
    return 0;
}

const struct lisa_ui_nav_scr setting_wifi_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_SETTING_WIFI,
    .open = setting_wifi_nav_scr_open,
    .show = setting_wifi_nav_scr_show,
    .pause = NULL,
    .resume = NULL,
    .close = setting_wifi_nav_scr_close,
};
