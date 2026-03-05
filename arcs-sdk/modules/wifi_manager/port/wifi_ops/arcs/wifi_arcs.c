#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "wifi_api.h"
#include "cache.h"
#include "ls_event.h"

#include "wifi_manager/dlist.h"
#include "wifi_manager/wifi_manager_wifi_ops.h"
#include "wifi_manager/wifi_manager.h"

#include "wifi_manager/priv/platform_dev.h"

// 类型转换函数声明

#define TAG                  "wifi_arcs"
#include "lisa_log.h"

#define WIFI_SCAN_MAX_NUMBER (32)
#define WIFI_CONNECT_RETRY_TIMES    3           //wifi lib连接失败后重试次数

#define GROUP_EVENTS_WIFI_STA_CONNECT_FINISH  (1<<0)
#define GROUP_EVENTS_WIFI_STA_CONNECT_FAILED  (1<<1)

static wifi_mgr_wifi_sta_config_t s_sta_config;
static wifi_mgr_wifi_sta_config_t s_connected_config;
static EventGroupHandle_t wifi_events;
static int32_t connect_retry = WIFI_CONNECT_RETRY_TIMES;  //连上网的重试次数
static sys_dlist_t wifi_callback_list;
static bool s_sta_connected;

static inline wifi_mgr_wifi_encryption_mode_t convert_encryption_mode(wifi_security_e auth);
static wifi_security_e convert_to_ls_wifi_type(wifi_mgr_wifi_encryption_mode_t mode);
static void arcs_wifi_update_connected_info(wifi_mgr_wifi_sta_config_t *sta);
static bool arcs_wifi_parse_bssid(const char *bssid_str, uint8_t *bssid);
static uint16_t arcs_wifi_channel_to_freq(int channel);

void dispatch_callbacks(wifi_mgr_wifi_event_t events, void *event_data, uint32_t event_data_len)
{
    sys_dlist_t *list = &wifi_callback_list;
    wifi_mgr_wifi_event_cb_t *cb, *tmp;
    SYS_DLIST_FOR_EACH_CONTAINER_SAFE(list, cb, tmp, node) {
        if ((cb->events & events) && (cb->handler != NULL)) {
            cb->handler(cb->events & events, event_data, event_data_len, cb->arg);
        }
    }
}
static int _wifi_event_wrap(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    event_connect_fail_param_t *conn_fail_evt;
    wifi_mgr_connect_fail_info_t fail_info;

    switch (event_id) {
    case EVENT_WIFI_CONNECTED:
        arcs_wifi_update_connected_info(&s_sta_config);
        memcpy(&s_connected_config, &s_sta_config, sizeof(s_connected_config));
        s_sta_connected = true;
        dispatch_callbacks(WIFI_MGR_WIFI_EVT_STA_CONNECTED, &s_connected_config, sizeof(s_connected_config));
        xEventGroupSetBits(wifi_events, GROUP_EVENTS_WIFI_STA_CONNECT_FINISH);
        break;
    case EVENT_WIFI_DISCONNECT:
    case EVENT_WIFI_STA_CONNECT_FAIL:
        conn_fail_evt = (event_connect_fail_param_t *)event_data;
        if (conn_fail_evt == NULL) {
            fail_info.error_code = -1;
            fail_info.status_code = -1;
            fail_info.reason_code = -1;
        } else {
            fail_info.error_code = conn_fail_evt->erro_code;
            fail_info.status_code = conn_fail_evt->status_code;
            fail_info.reason_code = conn_fail_evt->reason_code;
        }
        if (s_sta_connected) {
            s_sta_connected = false;
            wifi_mgr_disconnect_event_info_t disconnect_info = {0};
            memcpy(&disconnect_info.sta_config, &s_connected_config, sizeof(disconnect_info.sta_config));
            memcpy(&disconnect_info.fail_info, &fail_info, sizeof(disconnect_info.fail_info));
            LOGI("wifi disconnected, error_code:%d, status_code:%d, reason_code:%d",
                 fail_info.error_code, fail_info.status_code, fail_info.reason_code);
            dispatch_callbacks(WIFI_MGR_WIFI_EVT_STA_DISCONNECTED, &disconnect_info, sizeof(disconnect_info));
        } else {
            LISA_LOGI(TAG, "[%s %d], err:%d, status:%d, reason:%d",__FUNCTION__,__LINE__,
                     fail_info.error_code, fail_info.status_code, fail_info.reason_code);
            dispatch_callbacks(WIFI_MGR_WIFI_EVT_STA_CONNECTION_FAILED, &fail_info, sizeof(fail_info));
            xEventGroupSetBits(wifi_events, GROUP_EVENTS_WIFI_STA_CONNECT_FAILED);
        }
        break;
    default:
        LISA_LOGW(TAG, "[%s %d]unknow wifi event %d",__FUNCTION__,__LINE__, event_id);
        break;
    }
    return 0;
}

static void arcs_wifi_update_connected_info(wifi_mgr_wifi_sta_config_t *sta)
{
    if (sta == NULL) {
        return;
    }
    // ARCS 平台特有：在连接成功后读取 PMK 并填充到 s_sta_config
    uint8_t pmk[32];
    if (wifi_get_pmk(pmk) == LS_OK) {
        memcpy(sta->pmk, pmk, 32);
        sta->pmk_valid = 1;
        LISA_LOGI(TAG, "PMK cached in sta_config");
    } else {
        sta->pmk_valid = 0;
        LISA_LOGW(TAG, "Failed to get PMK from hardware");
    }
    // 连接成功后同步 BSSID 和信道信息
    struct wifi_link_status link_status;
    if (wifi_get_link_status(&link_status) == LS_OK) {
        snprintf(sta->bssid, sizeof(sta->bssid), "%02x:%02x:%02x:%02x:%02x:%02x", link_status.bssid[0],
                 link_status.bssid[1], link_status.bssid[2], link_status.bssid[3], link_status.bssid[4],
                 link_status.bssid[5]);
        sta->channel = link_status.channel;
    }
}

int arcs_wifi_init(void)
{
    wifi_events = xEventGroupCreate();
    s_sta_connected = false;
    memset(&s_sta_config, 0, sizeof(s_sta_config));
    memset(&s_connected_config, 0, sizeof(s_connected_config));
    sys_dlist_init(&wifi_callback_list);
    ls_event_register_cb(EVENT_WIFI, EVENT_WIFI_CONNECTED, _wifi_event_wrap, NULL);
    ls_event_register_cb(EVENT_WIFI, EVENT_WIFI_DISCONNECT, _wifi_event_wrap, NULL);
    ls_event_register_cb(EVENT_WIFI, EVENT_WIFI_STA_CONNECT_FAIL, _wifi_event_wrap, NULL);
    return 0;
};

int arcs_wifi_deinit(void)
{
    ls_event_unregister_cb(EVENT_WIFI, EVENT_WIFI_CONNECTED, _wifi_event_wrap);
    ls_event_unregister_cb(EVENT_WIFI, EVENT_WIFI_DISCONNECT, _wifi_event_wrap);
    ls_event_unregister_cb(EVENT_WIFI, EVENT_WIFI_STA_CONNECT_FAIL, _wifi_event_wrap);
    vEventGroupDelete(wifi_events);
    return 0;
};

int arcs_wifi_add_callback(wifi_mgr_wifi_event_cb_t *wifi_event_cb)
{
    if (wifi_event_cb == NULL) {
        return -EINVAL;
    }

    sys_dlist_prepend(&wifi_callback_list, &wifi_event_cb->node);
    return 0;
};

int arcs_wifi_remove_callback(wifi_mgr_wifi_event_cb_t *wifi_event_cb)
{
    int ret = -ENXIO;
    sys_dlist_t *list = &wifi_callback_list;
    wifi_mgr_wifi_event_cb_t *cb, *tmp;
    SYS_DLIST_FOR_EACH_CONTAINER_SAFE(list, cb, tmp, node)
    {
        if (cb == wifi_event_cb) {
            sys_dlist_remove(&cb->node);
            ret = 0;
            break;
        }
    }
    return ret;
}

int arcs_wifi_sta_enable(void)
{
    return wifi_sta_mode_enable();
}

int arcs_wifi_sta_disable(void)
{
    return wifi_sta_mode_disable();
}

bool arcs_wifi_sta_is_enable(void)
{
    wifi_mode_e mode;
    wifi_get_mode(WIFI_VIF_DEFAULT_IDX, &mode);
    return (mode == WIFI_MODE_STA);
}

int arcs_wifi_scan_ap(wifi_mgr_wifi_scan_info_t *ap_info, uint32_t size, uint32_t timeout_ms)
{
    int ret;
    int i;
    wifi_scan_params_t* p_scan_param = NULL;
    wifi_scan_result_t *p_scan_results = NULL;
    int nb_res;
    int* p_nb_res = NULL;
    wifi_mgr_scan_ap_list_t dev_scan_ap_list;

    p_scan_param = PLATFORM_MEM_NOCACHE_MALLOC(sizeof(wifi_scan_params_t));
    p_nb_res = PLATFORM_MEM_NOCACHE_MALLOC(sizeof(int));
    if((p_scan_param == NULL) || (p_nb_res == NULL)){
        LISA_LOGE(TAG,"[%s %d]malloc failed",__FUNCTION__,__LINE__);
        return -ENOMEM;
    }

    memset(p_scan_param, 0, sizeof(wifi_scan_params_t));
    ret = wifi_scan_start(p_scan_param);
    if (ret != LS_OK) {
        LISA_LOGE(TAG, "Scan error %d\n", ret);
        ret = -EIO;
        goto _cleanup;
    }

    ls_event_wait(EVENT_WIFI, EVENT_WIFI_SCAN_DONE, LS_NEVER_TIMEOUT);
    // as scan_results will be write on the other core and read on this core,
    // should use rtos_aligned_malloc, aligned by HAL_DCACHE_CFG_LINE_SIZE
    p_scan_results = PLATFORM_MEM_ALIGN_MALLOC(HAL_DCACHE_CFG_LINE_SIZE, sizeof(wifi_scan_result_t) * size);
    if (!p_scan_results) {
        LISA_LOGE(TAG, "[%s %d]malloc size:%d failed", __FUNCTION__, __LINE__, sizeof(wifi_scan_result_t) * size);
        ret = -ENOMEM;
        goto _cleanup;
    }
    wifi_sta_scanlist_dump(p_scan_results, size, p_nb_res);
    HAL_InvalidateDCache_by_Addr((uint32_t *)p_scan_results, sizeof(wifi_scan_result_t) * size);
    nb_res = *p_nb_res;
    LISA_LOGI(TAG, "Got %d scan results,ret:%d\n", *p_nb_res, ret);

    for (i = 0; i < *p_nb_res; i++) {
        snprintf(ap_info[i].ssid, sizeof(ap_info[i].ssid), "%s", p_scan_results[i].ssid);
        snprintf(ap_info[i].bssid, sizeof(ap_info[i].bssid), "%02x:%02x:%02x:%02x:%02x:%02x", p_scan_results[i].bssid[0],
                 p_scan_results[i].bssid[1], p_scan_results[i].bssid[2], p_scan_results[i].bssid[3], p_scan_results[i].bssid[4],
                 p_scan_results[i].bssid[5]);
        ap_info[i].rssi = p_scan_results[i].rssi;
        ap_info[i].channel = p_scan_results[i].channel;
        ap_info[i].encryption_mode = convert_encryption_mode(p_scan_results[i].auth);
    }
    dev_scan_ap_list.ap_info = ap_info;
    dev_scan_ap_list.count = nb_res;
    dispatch_callbacks(WIFI_MGR_WIFI_EVT_SCAN_DONE, &dev_scan_ap_list, sizeof(dev_scan_ap_list));

_cleanup:
    if(p_scan_param){
        PLATFORM_MEM_FREE(p_scan_param);
    }

    if(p_scan_results){
        PLATFORM_MEM_FREE(p_scan_results);
    }

    if(p_nb_res){
        PLATFORM_MEM_FREE(p_nb_res);
    }
    if(ret < 0){
        return ret;
    }
    return nb_res;
};

int arcs_wifi_sta_connect(wifi_mgr_wifi_sta_config_t *sta_config, uint32_t timeout_ms)
{
    wifi_connect_cfg_t* pcfg;
    EventBits_t uxBits;
    int ret;

    pcfg = PLATFORM_MEM_NOCACHE_MALLOC(sizeof(wifi_connect_cfg_t));
    if(pcfg == NULL){
        LISA_LOGE(TAG,"[%s %d]malloc failed",__FUNCTION__,__LINE__);
        return -ENOMEM;
    }
   
    memset(pcfg, 0, sizeof(wifi_connect_cfg_t));

    pcfg->failure_retry_cnt = connect_retry;
    pcfg->sec = convert_to_ls_wifi_type(sta_config->encryption_mode);
    snprintf((char*)pcfg->ssid, sizeof(pcfg->ssid), "%s", sta_config->ssid);
    snprintf((char*)pcfg->key, sizeof(pcfg->key), "%s", sta_config->pwd);
    memcpy(&s_sta_config,sta_config,sizeof(wifi_mgr_wifi_sta_config_t));
    uint16_t freq = arcs_wifi_channel_to_freq(sta_config->channel);
    if (freq != 0) {
        pcfg->freq[0] = freq;
        pcfg->freq[1] = freq;
    }
    if (sta_config->bssid[0] != '\0') {
        arcs_wifi_parse_bssid(sta_config->bssid, pcfg->bssid);
    }

    // ARCS 平台特有：在连接前恢复 PMK 到硬件
    if (sta_config->pmk_valid) {
        ls_err_t pmk_ret = wifi_set_pmk(sta_config->pmk);
        if (pmk_ret == LS_OK) {
            LISA_LOGI(TAG, "PMK restored to hardware for fast reconnection");
        } else {
            LISA_LOGW(TAG, "Failed to restore PMK: %d, will use normal connect", pmk_ret);
            // PMK 恢复失败不影响连接，继续正常连接流程
        }
    }

    xEventGroupClearBits(wifi_events, GROUP_EVENTS_WIFI_STA_CONNECT_FINISH | GROUP_EVENTS_WIFI_STA_CONNECT_FAILED);

    LISA_LOGD(TAG, "Calling wifi_sta_connect...");
    LISA_LOGD(TAG, "BSSID: %02x:%02x:%02x:%02x:%02x:%02x",
              pcfg->bssid[0], pcfg->bssid[1], pcfg->bssid[2],
              pcfg->bssid[3], pcfg->bssid[4], pcfg->bssid[5]);
    LISA_LOGD(TAG, "SSID: %s", pcfg->ssid);
    LISA_LOGD(TAG, "Key: %s", pcfg->key);
    LISA_LOGD(TAG, "Freq: %u/%u", pcfg->freq[0], pcfg->freq[1]);
    LISA_LOGD(TAG, "PMK valid: %d", sta_config->pmk_valid);
    if (sta_config->pmk_valid) {
        LISA_LOGD(TAG, "PMK: %02X:%02X:%02X:%02X:%02X:%02X", sta_config->pmk[0], sta_config->pmk[1], sta_config->pmk[2], sta_config->pmk[3], sta_config->pmk[4], sta_config->pmk[5]);
    }

    ret = wifi_sta_connect(pcfg);
    if (ret != LS_OK) {
        LISA_LOGE(TAG, "WIFI connect failed,err%d \r\n", ret);
        ret = -EIO;
    }
    uxBits = xEventGroupWaitBits(wifi_events, GROUP_EVENTS_WIFI_STA_CONNECT_FINISH | GROUP_EVENTS_WIFI_STA_CONNECT_FAILED, true, false, pdMS_TO_TICKS(timeout_ms));
    if(uxBits & GROUP_EVENTS_WIFI_STA_CONNECT_FAILED) {
        ret = -EIO;
    }
    else if (!(uxBits & GROUP_EVENTS_WIFI_STA_CONNECT_FINISH)) {
        ret = -ETIMEDOUT;
    }

_cleanup:
    if(pcfg != NULL){
        PLATFORM_MEM_FREE(pcfg);
    }
    return ret;
}

int arcs_wifi_sta_disconnect(uint32_t timeout_ms)
{

    int ret;

    ret = wifi_sta_disconnect();
    if (ret != LS_OK) {
        LISA_LOGE(TAG, "WIFI disconnect failed,err%d \r\n", ret);
        return -EIO;
    }

    return 0;
}


wifi_mgr_wifi_status_t arcs_wifi_sta_get_status(void)
{
    int ret;
    wifi_link_status_t* p_link_status;
    int state = WIFI_MGR_WIFI_STATUS_STA_UNKNOWN;

    p_link_status = PLATFORM_MEM_NOCACHE_MALLOC(sizeof(wifi_link_status_t));
    if(p_link_status == NULL){
        LISA_LOGE(TAG,"[%s %d]malloc failed",__FUNCTION__,__LINE__);
        return -ENOMEM;
    }

    ret = wifi_get_link_status(p_link_status);

    if (ret) {
        LISA_LOGE(TAG,"WIFI get link status failed,ret:%d\r\n", ret);
        return WIFI_MGR_WIFI_STATUS_STA_UNKNOWN;
    } else {
        LISA_LOGI(TAG,"WIFI get link status:%d\r\n", p_link_status->state);
        switch (p_link_status->state) {
        case STA_INACTIVE:
            state = WIFI_MGR_WIFI_STATUS_STA_DISCONNECTED;
            break;
        case STA_IN_CONNECTING:
            state = WIFI_MGR_WIFI_STATUS_STA_DISCONNECTED;
            break;
        case STA_CONNECTED:
            state = WIFI_MGR_WIFI_STATUS_STA_CONNECTED;
            break;
        case STA_DISCONNECTED:
            state = WIFI_MGR_WIFI_STATUS_STA_DISCONNECTED;
            break;
        default:
            LISA_LOGE(TAG, "Not in STA mode \r\n");
            break;
        }
    }

    if(p_link_status != NULL){
        PLATFORM_MEM_FREE(p_link_status);
    }

    return state;
}

int arcs_wifi_get_mac(uint8_t *mac_addr)
{
    int ret;
    uint8_t *p_mac;

    p_mac = PLATFORM_MEM_NOCACHE_MALLOC(sizeof(uint8_t)*6);
    if(p_mac == NULL){
        LISA_LOGE(TAG,"[%s %d]malloc failed",__FUNCTION__,__LINE__);
        return -ENOMEM;
    }

    ret = wifi_get_sta_mac(p_mac);
    memcpy(mac_addr,p_mac,6);

    if(p_mac != NULL){
        PLATFORM_MEM_FREE(p_mac);
    }

    if (ret) {
        LISA_LOGE(TAG, "WIFI get sta mac failed,err:%d", ret);
        return -EIO;
    }


    return 0;
}

static wifi_security_e convert_to_ls_wifi_type(wifi_mgr_wifi_encryption_mode_t mode)
{
    wifi_security_e auth = WIFI_SEC_UNKNOWN;

    LISA_LOGI(TAG,"[%s %d]mode:%d", __FUNCTION__, __LINE__, mode);

    switch (mode) {
    case WIFI_MGR_WIFI_AUTH_AUTO:
        auth = WIFI_SEC_AUTO;
        break;
    case WIFI_MGR_WIFI_AUTH_OPEN:
        auth = WIFI_SEC_OPEN;
        break;
    case WIFI_MGR_WIFI_AUTH_WEP:
        auth = WIFI_SEC_WEP;
        break;
    case WIFI_MGR_WIFI_AUTH_WPA_PSK:
        auth = WIFI_SEC_WPA_PSK;
        break;
    case WIFI_MGR_WIFI_AUTH_WPA2_PSK:
        auth = WIFI_SEC_WPA2_PSK;
        break;
    case WIFI_MGR_WIFI_AUTH_WPA_WPA2_PSK:
        auth = WIFI_SEC_WPA_PSK_WPA2_PSK;
        break;
    case WIFI_MGR_WIFI_AUTH_WPA2_ENTERPRISE:
        auth = WIFI_SEC_WPA_ENTERPRISE;
        break;
    case WIFI_MGR_WIFI_AUTH_WPA3_PSK:
        auth = WIFI_SEC_WPA3_SAE;
        break;
    case WIFI_MGR_WIFI_AUTH_UNKNOWN:
        auth = WIFI_SEC_UNKNOWN;
        break;
    default:
        break;
    }

    return auth;
}

static inline wifi_mgr_wifi_encryption_mode_t convert_encryption_mode(wifi_security_e auth)
{

    wifi_mgr_wifi_encryption_mode_t mode = WIFI_MGR_WIFI_AUTH_UNKNOWN;

    switch (auth) {
    case WIFI_SEC_AUTO:
        mode = WIFI_MGR_WIFI_AUTH_AUTO;
        break;
    case WIFI_SEC_OPEN:
        mode = WIFI_MGR_WIFI_AUTH_OPEN;
        break;
    case WIFI_SEC_WEP:
        mode = WIFI_MGR_WIFI_AUTH_WEP;
        break;
    case WIFI_SEC_WPA_PSK:
        mode = WIFI_MGR_WIFI_AUTH_WPA_PSK;
        break;
    case WIFI_SEC_WPA2_PSK:
        mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;
        break;
    case WIFI_SEC_WPA_PSK_WPA2_PSK:
        mode = WIFI_MGR_WIFI_AUTH_WPA_WPA2_PSK;
        break;
    case WIFI_SEC_WPA_ENTERPRISE:
        mode = WIFI_MGR_WIFI_AUTH_WPA2_ENTERPRISE;
        break;
    case WIFI_SEC_WPA3_SAE:
        mode = WIFI_MGR_WIFI_AUTH_WPA3_PSK;
        break;
    case WIFI_SEC_WPA2_PSK_WPA3_SAE:
        mode = WIFI_MGR_WIFI_AUTH_UNKNOWN;
        break;
    default:
        break;
    }

    return mode;
}

static uint16_t arcs_wifi_channel_to_freq(int channel)
{
    if (channel < 1 || channel > 14) {
        return 0;
    }
    return (uint16_t)(2412 + (channel - 1) * 5);
}

static bool arcs_wifi_parse_bssid(const char *bssid_str, uint8_t *bssid)
{
    unsigned int parts[6];
    if (bssid_str == NULL || bssid == NULL) {
        return false;
    }
    if (sscanf(bssid_str, "%02x:%02x:%02x:%02x:%02x:%02x",
               &parts[0], &parts[1], &parts[2],
               &parts[3], &parts[4], &parts[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        if (parts[i] > 0xFF) {
            return false;
        }
        bssid[i] = (uint8_t)parts[i];
    }
    return true;
}


// WiFi Manager 操作结构体
static wifi_manager_wifi_ops_t arcs_mgr_wifi_ops = {
    .init = arcs_wifi_init,
    .deinit = arcs_wifi_deinit,
    .sta_enable = arcs_wifi_sta_enable,
    .sta_disable = arcs_wifi_sta_disable,
    .sta_is_enable = arcs_wifi_sta_is_enable,
    .add_callback = arcs_wifi_add_callback,
    .remove_callback = arcs_wifi_remove_callback,
    .scan_ap = arcs_wifi_scan_ap,
    .sta_connect = arcs_wifi_sta_connect,
    .sta_disconnect = arcs_wifi_sta_disconnect,
    .sta_get_status = arcs_wifi_sta_get_status,
    .get_mac = arcs_wifi_get_mac,
};

// 获取 WiFi Manager 操作接口
wifi_manager_wifi_ops_t* wifi_manager_arcs_wifi_ops_get(void)
{
    return &arcs_mgr_wifi_ops;
}
