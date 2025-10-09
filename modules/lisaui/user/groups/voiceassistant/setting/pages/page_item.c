#include <stdint.h>
#include "lvgl.h"
#include "platform.h"
#include "lisaui_common.h"
#include "assets/assets_res.h"
#include "lisaui_stack_page.h"
#include "lisaui_group.h"
#include "lisaui_manager.h"

#include "../group_setting.h"
#include "setting_pages.h"
#include "lisaui_log.h"
#include "user_groups.h"
#include "lisa_ui_assets.h"

#define TAG "setting.page.main"

#include "lisa_ui_scr_setting_main.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_scr_setting_net.h"
#include "lisa_ui_scr_setting_common.h"
#include "lisa_ui_scr_setting_wakeup.h"
#include "lisa_ui_scr_kb_input.h"
#include "lisa_ui_toast.h"
#include "ebus/ebus.h"
#include "lisaui_user_data.h"
#include "page_item.h"

typedef struct {
    ebus_chn_t *ebus_ch_base_event;
    uint32_t item_index;
    struct setting_item **item;
} page_data_t;


static lv_obj_t *setting_net_create(lv_obj_t *parent,void* userdata);
static lv_obj_t *setting_common_create(lv_obj_t *parent,void* userdata);
struct setting_item *setting_items[SETTING_ITEM_MAX_NUMBER] = {
    [SETTING_ITEM_WIFI_INDEX] = &(struct setting_item){"网络设置", &LISA_UI_ASSETS_IMG_DSC(img_png_setting_wifi), NULL,
                                                        setting_net_create},
    [SETTING_ITEM_COMMON_INDEX] = &(struct setting_item){"基础设置", &LISA_UI_ASSETS_IMG_DSC(img_png_setting_regular),
                                                         NULL, setting_common_create},
};

static void setting_item_click_cb(const char *name, void *user_data)
{
    LISAUI_LOGI(TAG, "Setting item click:%s", name);
    for (int i = 0; i < SETTING_ITEM_MAX_NUMBER; i++) {
        if (0 == strcmp(name, setting_items[i]->name)) {
            lisa_ui_scr_base_show(setting_items[i]->scr);
            break;
        }
    }
}

static void net_wifi_connect_cb(const char *ssid, const char *pwd, bool is_encrypted, bool is_mine, void *user_data)
{

    lisaui_page_t *page = (lisaui_page_t *)user_data;
    page_data_t *data = (page_data_t *)page->page_data;
    LISAUI_LOGI(TAG, "net_wifi_connect_cb, ssid:%s, pwd:%s, is_encrypted:%d, is_mine:%d", ssid, pwd, is_encrypted,
                is_mine);
    lisaui_userdata_setting_wifi_connect_info_t *connect_info;
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        connect_info = &_userdata->setting.wifi.connect_info;
        snprintf(connect_info->cfg.ssid, sizeof(connect_info->cfg.ssid), "%s", ssid);
        snprintf(connect_info->cfg.pwd, sizeof(connect_info->cfg.pwd), "%s", pwd);
        connect_info->state = LISAUI_USERDATA_WIFI_CONNECT_STATE_DISCONNECTED;
    }
    ebus_message_pub(data->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_WIFI_CONNECT, NULL, 0);
}

static void wifi_switch_cb(lv_event_t *event)
{

    lv_obj_t *obj = lv_event_get_target(event);
    lv_event_code_t code = lv_event_get_code(event);
    uint32_t state = *(uint32_t *)lv_event_get_param(event);
    lisaui_page_t *page = (lisaui_page_t *)lv_event_get_user_data(event);
    page_data_t *data = (page_data_t *)page->page_data;

    if (code == LV_EVENT_VALUE_CHANGED) {
        
        if (state & LV_STATE_CHECKED) {
            ebus_message_pub(data->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_WIFI_ENABLE, NULL, 0);
        } else {
            ebus_message_pub(data->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_WIFI_DISABLE, NULL, 0);
        }
        LISAUI_LOGI(TAG, "new_wifi_switch_cb, code :%d, obj:%p, state:%d\r\n", code, obj, state);
    }
}

static void mic_again_value_changed(lv_event_t *event){
    
    lisaui_page_t *page = (lisaui_page_t *)lv_event_get_user_data(event);
    page_data_t *data = (page_data_t *)page->page_data;
    uint32_t value = *(uint32_t *)lv_event_get_param(event);
    LISAUI_LOGI(TAG, "mic_again_value_changed value:%d",value);
    LISAUI_USERDATA_WITH_LOCK(_userdata){
        _userdata->setting.mic_gain = value > 100?100:value;
     }
     ebus_message_pub(data->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_SETTING_MIC_GAIN_UPDATE, NULL, 0);
}

static void volume_value_changed(lv_event_t *event){
    
    lisaui_page_t *page = (lisaui_page_t *)lv_event_get_user_data(event);
    page_data_t *data = (page_data_t *)page->page_data;
    uint32_t value = *(uint32_t *)lv_event_get_param(event);
    LISAUI_LOGI(TAG, "volume_value_changed value:%d",value);
    LISAUI_USERDATA_WITH_LOCK(_userdata){
        _userdata->setting.volume_percent = value > 100?100:value;
     }
     ebus_message_pub(data->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_SETTING_VOL_UPDATE, NULL, 0);
}

static inline uint8_t wifi_rssi_to_level(int rssi)
{
   if (rssi >= -50)       return 5;  // 非常强 (-50dBm 及以上)
   else if (rssi >= -60)  return 4;  // 强     (-60dBm 到 -51dBm)
   else if (rssi >= -70)  return 3;  // 中等   (-70dBm 到 -61dBm)
   else if (rssi >= -80)  return 2;  // 弱     (-80dBm 到 -71dBm)
   else                   return 1;  // 非常弱 (-81dBm 及以下)
}
static int event_wifi_update_handler(ebus_chn_t *chn, uint32_t code, void *message, uint32_t msg_size, void *user_data)
{

    lisaui_page_t *page = (lisaui_page_t *)user_data;
    page_data_t *data = (page_data_t *)page->page_data;

    LISAUI_LOGI(TAG, "event_wifi_update_handler, code:%d, user_data:%p\r\n", code, user_data);

    if (lisa_ui_scr_setting_net_is_connecting(data->item[SETTING_ITEM_WIFI_INDEX]->scr)) {
        LISAUI_LOGI(TAG, "event_wifi_update_handler, wifi is connecting");
        return 0;
    }

    lisa_ui_scr_setting_net_wifi_list_clear(data->item[SETTING_ITEM_WIFI_INDEX]->scr);
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        if(_userdata->setting.wifi.is_enable == true){

            lisa_ui_scr_setting_net_switch_add_state(data->item[SETTING_ITEM_WIFI_INDEX]->scr,LV_STATE_CHECKED);
            for (int i = 0;  _userdata->setting.wifi.p_hotspot_info && (i < _userdata->setting.wifi.p_hotspot_info->number); i++) {
            
                bool is_encrypted = true;
                uint8_t rssi_level = wifi_rssi_to_level(_userdata->setting.wifi.p_hotspot_info->hotspot[i].rssi);
                uint8_t connect_state = (_userdata->setting.wifi.p_hotspot_info->hotspot[i].state == LISAUI_USERDATA_WIFI_HOTSPOT_STATE_CONNECTED) ? 
                                        2 : (_userdata->setting.wifi.p_hotspot_info->hotspot[i].state == LISAUI_USERDATA_WIFI_HOTSPOT_STATE_CONNECTING) ? 1 : 0;
                if (strcmp(_userdata->setting.wifi.p_hotspot_info->hotspot[i].encryption_mode_str, "OPEN") == 0) {
            
                    is_encrypted = false;
                }
    
                if (_userdata->setting.wifi.p_hotspot_info->hotspot[i].state ==
                    LISAUI_USERDATA_WIFI_HOTSPOT_STATE_CONNECTED) {
            
                        lisa_ui_wifi_item_add_to_list_mine(data->item[SETTING_ITEM_WIFI_INDEX]->scr,
                                                       _userdata->setting.wifi.p_hotspot_info->hotspot[i].ssid,
                                                       is_encrypted,
                                                       rssi_level,
                                                       connect_state,
                                                       net_wifi_connect_cb, page);
    
                } else {
            
                    lisa_ui_wifi_item_add_to_list_other(data->item[SETTING_ITEM_WIFI_INDEX]->scr,
                                                        _userdata->setting.wifi.p_hotspot_info->hotspot[i].ssid,
                                                        is_encrypted,
                                                        rssi_level,
                                                        connect_state,
                                                        net_wifi_connect_cb, page);
                }
            }
        }else{
            
            lisa_ui_scr_setting_net_switch_clear_state(data->item[SETTING_ITEM_WIFI_INDEX]->scr, LV_STATE_CHECKED);
        }
        
    }
}


static lv_obj_t *setting_net_create(lv_obj_t *parent,void* userdata){

    lv_obj_t *scr;

    scr = lisa_ui_scr_setting_net_create(parent);
    lv_obj_add_event_cb(scr, wifi_switch_cb, LV_EVENT_VALUE_CHANGED, userdata);
    LISAUI_USERDATA_WITH_LOCK(_userdata){
        if(_userdata->setting.wifi.is_enable == true){
            lisa_ui_scr_setting_net_switch_add_state(scr, LV_STATE_CHECKED);
        }
        else{
            lisa_ui_scr_setting_net_switch_clear_state(scr, LV_STATE_CHECKED);
        }
    }

    return scr;
}


static lv_obj_t *setting_common_create(lv_obj_t *parent,void* userdata){

    lv_obj_t *scr;

    scr = lisa_ui_scr_setting_common_create(parent);
    lisa_ui_scr_setting_common_add_event_cb(scr, 
                                                    mic_again_value_changed, 
                                                    LISA_UI_SCR_SETTING_COMMON_MIC_AGAIN_SLIDER_VALUE_CHANGED_EVENT,
                                                    userdata);
    lisa_ui_scr_setting_common_add_event_cb(scr, 
                                                    volume_value_changed, 
                                                    LISA_UI_SCR_SETTING_COMMON_VOL_SLIDER_VALUE_CHANGED_EVENT,
                                                    userdata);
    LISAUI_USERDATA_WITH_LOCK(_userdata){
        lisa_ui_scr_setting_common_mic_again_slider_value_set(scr, _userdata->setting.mic_gain);
        lisa_ui_scr_setting_common_vol_slider_value_set(scr, _userdata->setting.volume_percent);
    }    

    return scr;
    
}

static lisaui_page_t *create(lisaui_page_t *page)
{

    page_data_t *data = NULL;
    data = lisaui_malloc(sizeof(page_data_t));
    if (data == NULL) {

        LISAUI_LOGE(TAG, "[%s %d]no memory!", __FUNCTION__, __LINE__);
        goto _ERR;
    }
    data->item_index = 0;
    data->item = setting_items;
    for (int i = 0; i < SETTING_ITEM_MAX_NUMBER; i++) {

        data->item[i]->scr = data->item[i]->create(NULL,page);
        lisa_ui_scr_base_title_set(data->item[i]->scr, data->item[i]->name);
    }

    data->ebus_ch_base_event = ebus_chn_bind(LISAUI_EBUS_NAME, LISAUI_EBUS_CH_BASE_EVENT_NAME);
    if (data->ebus_ch_base_event != NULL) {
        ebus_message_subscribe(data->ebus_ch_base_event, EBUS_SUBSCRIBER_TYPE_SYNC,
                               LISAUI_EBUS_CH_EVENT_M2U_SETTING_WIFI_UPDATE, event_wifi_update_handler, page);
    }

    page->page_data = data;

    LISAUI_LOGI(TAG, "Create %s page success", page->cname);
    return page;

_ERR:
    return NULL;
}

static lisaui_err_t destroy(lisaui_page_t *page)
{
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for destroy");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (page->page_data) {
        page_data_t *data = (page_data_t *)page->page_data;
        ebus_message_unsubscribe(data->ebus_ch_base_event, event_wifi_update_handler);
        for (int i = 0; i < SETTING_ITEM_MAX_NUMBER; i++) {
            lisa_ui_scr_base_del(data->item[i]->scr);
        }

        lisaui_free(page->page_data);
        page->page_data = NULL;
    }

    LISAUI_LOGI(TAG, "Destroy %s page", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t show(lisaui_page_t *page)
{

    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for show");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (page->page_data) {

        page_data_t *data = (page_data_t *)page->page_data;
        if (data->item_index < SETTING_ITEM_MAX_NUMBER) {
            if (data->item[data->item_index]->scr != NULL) {
                lisa_ui_scr_base_show(data->item[data->item_index]->scr);
            }
        }
    }

    LISAUI_LOGI(TAG, "Show %s page", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t close(lisaui_page_t *page)
{

    if (!page) {

        LISAUI_LOGE(TAG, "Invalid page pointer for close");
        return LISAUI_ERR_INVALID_PARAM;
    }
    if (page->page_data) {

        page_data_t *data = (page_data_t *)page->page_data;
        if (data->item_index < SETTING_ITEM_MAX_NUMBER) {
            if (data->item[data->item_index]->scr != NULL) {
                lisa_ui_scr_base_hide(data->item[data->item_index]->scr);
            }
        }
    }
    LISAUI_LOGI(TAG, "Close %s page", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t update_data(lisaui_page_t *page, void *data)
{

    page_item_data_t *item_data = (page_item_data_t *)data;
    if (!item_data) {

        LISAUI_LOGE(TAG, "Invalid page data for update");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (item_data->item_index >= SETTING_ITEM_MAX_NUMBER) {

        LISAUI_LOGE(TAG, "Invalid item index for update");
        return LISAUI_ERR_INVALID_PARAM;
    }
    if (page && page->page_data) {
        page_data_t *data = (page_data_t *)page->page_data;
        lisa_ui_scr_base_show(data->item[item_data->item_index]->scr);
        data->item_index = item_data->item_index;
        if (SETTING_ITEM_WIFI_INDEX == item_data->item_index) {
            ebus_message_pub(data->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_WIFI_SCAN, NULL, 0);
        }
    }

    LISAUI_LOGI(TAG, "Update %s page data", page->cname);
    return LISAUI_ERR_OK;
}

static const lisaui_page_t setting_item_page = {
    .group_index = LISAUI_GROUP_INDEX_SETTING,
    .page_index = LISAUI_GROUP_SETTING_PAGE_INDEX_ITEM,
    .cname = "setting.item",
    .view = NULL,
    .page_data = NULL,
    .attribute = {.type = LISAUI_PAGE_TYPE_DATA_PAGE},
    .create = create,
    .destroy = destroy,
    .show = show,
    .close = close,
    .update_data = update_data,
};

LISAUI_PAGE_EXPORT(setting_item_page);
