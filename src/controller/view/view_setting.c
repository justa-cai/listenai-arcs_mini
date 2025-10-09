#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "sys/time.h"

#include "sysheap.h"

// #include "app_taskbar.h"
// #include "app_standby.h"
// #include "app_alarm.h"
// #include "app_wifi.h"
// #include "ls_llm_dialog.h"
// #include "app_setting_view_volume_backlight.h"
// #include "app_setting_view_wakeup_config.h"

#include "assistant_view.h"
#include "view_setting.h"

#define TAG "view_setting"
#include "lisa_log.h"

#define VIEW_ALARM_MAX_NUMBER (32)

view_setting_t *view_setting = NULL;

// static int setting_event_cb(lisaui_setting_item_t type, lisaui_setting_op_t operation, void *value);
// static void taskbar_event_cb(lisaui_taskbar_event_t event, void *param);
// static int setting_alarm_event_cb(lisaui_alarm_item_t type, lisaui_alarm_op_t operation, void *param);
// static lisaui_err_t setting_set_wakeup_mode_cb(lisaui_setting_wakeup_mode_op_t operation, void *param);
// static lisaui_err_t wifi_event_cb(lisaui_wifi_item_t type, lisaui_wifi_op_t op, void *params);

#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

int view_setting_init(void *view)
{

    // assistant_view_t* m_view = (assistant_view_t*)view;
    // if(view_setting != NULL){
    //     return -EBUSY;
    // }

    // memset(m_view,0,sizeof(view_setting_t));
    // view_setting = &m_view->setting;

    // lisaui_app_setting_register_handler(setting_event_cb);
    // lisaui_taskbar_register_event_handler(taskbar_event_cb);
    // lisaui_app_alarm_register_handler(setting_alarm_event_cb);
    // lisaui_app_setting_register_set_wakeup_mode_handler(setting_set_wakeup_mode_cb);
    // lisaui_app_wifi_register_event_handler(wifi_event_cb);

    return 0;
}

#if 0
static void taskbar_event_cb(lisaui_taskbar_event_t event, void *param){
    assistant_view_t* view;

    if(view_setting == NULL){
        return;
    }

    view = CONTAINER_OF(view_setting,assistant_view_t,setting);
    switch(event){
        case LISAUI_TASKBAR_EVENT_ENTER_SETTING:
            ls_llm_dialog_popup_with_mode(LISAUI_LLM_STATUS_FORCE_CLOSE, "", LISAUI_TEXT_MODE_OVERWRITE);
            lisaui_app_enter(UI_APP_ID_SETTING);
            break;
        case LISAUI_TASKBAR_EVENT_ENTER_STANDBY:
            lisaui_app_enter(UI_APP_ID_STANDBY);
            lisaui_app_standby_set_emoji(LISAUI_APP_STANDBY_EMOJI_TYPE_STANDBY);
            break;
        case LISAUI_TASKBAR_EVENT_BACK:
            break;
        default:
            break;
    }
    return;
}


static int setting_event_cb(lisaui_setting_item_t type, lisaui_setting_op_t operation, void *value){
    
    if(view_setting == NULL){
        return -ENODEV;
    }
    assistant_view_t* view = CONTAINER_OF(view_setting,assistant_view_t,setting);
    
    switch(type){
        case LISAUI_SETTING_ITEM_BACKLIGHT:
            if(operation == LISAUI_SETTING_OP_SET){
                view->cbs.display_backlight_set(*(uint8_t *)value);
            }
            else if(operation == LISAUI_SETTING_OP_GET){
                *(uint8_t*)value = view->cbs.display_backlight_get();
            }
            break;
        case LISAUI_SETTING_ITEM_VOLUME:
        
            if(operation == LISAUI_SETTING_OP_SET){
                view->cbs.audio_spk_volume_set(*(uint8_t *)value);
            }
            else if(operation == LISAUI_SETTING_OP_GET){
                *(uint8_t*)value = view->cbs.audio_spk_volume_get();
            }
            break;
        default:break;
    }
    
    return 0;
}

static int setting_alarm_event_cb(lisaui_alarm_item_t type, lisaui_alarm_op_t operation, void *param){

    int ret = 0;
    assistant_view_t* view;

    LISA_LOGI(TAG,"[%s %d]type:%d,opt:%d",__FUNCTION__,__LINE__,type,operation);
    if(view_setting == NULL){
        LISA_LOGW(TAG,"[%s %d]View not initialized");
        return -ENODEV;
    }

    view = CONTAINER_OF(view_setting,assistant_view_t,setting);

    if(operation == LISAUI_ALARM_OP_GET_LIST){
        struct tm tm = {0};
        int ii = 0;
        lisaui_alarm_clock_list_t* list = (lisaui_alarm_clock_list_t*)param;
        alarm_clock_list_t* m_alarm_list = exram_malloc(4,sizeof(alarm_clock_list_t) + sizeof(alarm_clock_t)*VIEW_ALARM_MAX_NUMBER);

        view->cbs.alarm_list_get(m_alarm_list,VIEW_ALARM_MAX_NUMBER);

        for(ii=0;ii<m_alarm_list->count && ii < list->count;ii++){
            

            localtime_r(&m_alarm_list->alarms[ii].timestamp, &tm);
            snprintf(list->list[ii].time_text, sizeof(list->list[ii].time_text), "%02d:%02d",
                    tm.tm_hour, tm.tm_min);
            snprintf(list->list[ii].date_text, sizeof(list->list[ii].date_text), "%2d月%d日",
                    tm.tm_mon + 1, tm.tm_mday);
            list->list[ii].timestamp = m_alarm_list->alarms[ii].timestamp;
            LISA_LOGI(TAG,"Get alarm list index:%d,timestamp:%lld",ii,list->list[ii].timestamp);

        }
        list->count = ii;

        exram_free(m_alarm_list);
        

    }
    else if(operation == LISAUI_ALARM_OP_DELETE){
        view->cbs.alarm_delete_by_timestamp(*(uint64_t*)param);

    }
    else{
        return -EPERM;
    }
    
    return ret;
}

static lisaui_err_t setting_set_wakeup_mode_cb(lisaui_setting_wakeup_mode_op_t operation, void *param) {
    int ret = 0;
    assistant_view_t* view;
    int view_mode = 0;
    
    LISA_LOGI(TAG,"[%s %d]operation:%d",__FUNCTION__,__LINE__,operation);
    if(view_setting == NULL){
        LISA_LOGW(TAG,"[%s %d]View not initialized");
        return -ENODEV;
    }

    view = CONTAINER_OF(view_setting,assistant_view_t,setting);

    if(operation == LISAUI_SETTING_WAKEUP_OP_SET){
        int mode = *(lisaui_app_setting_wakeup_mode_t *)param;
        LISA_LOGI(TAG,"[%s %d]set mode:%d", __FUNCTION__, __LINE__, mode);

        if(mode == LISAUI_APP_SETTING_WAKEUP_MODE_KEY){
            view_mode = 2;
        }
        else if(mode == LISAUI_APP_SETTING_WAKEUP_MODE_VOICE_SINGLE){
            view_mode = 0;
        }
        else if(mode == LISAUI_APP_SETTING_WAKEUP_MODE_VOICE_MULTI){
            view_mode = 1;
        }
        view->cbs.cloud_interactive_mode_set(view_mode);
    } else {
        
        view_mode = view->cbs.cloud_interactive_mode_get();
        LISA_LOGI(TAG,"[%s %d]get view_mode:%d", __FUNCTION__, __LINE__, view_mode);
        if(view_mode == 0){
            *(lisaui_app_setting_wakeup_mode_t *)param = LISAUI_APP_SETTING_WAKEUP_MODE_VOICE_SINGLE;
        }
        else if(view_mode == 1){
            *(lisaui_app_setting_wakeup_mode_t *)param = LISAUI_APP_SETTING_WAKEUP_MODE_VOICE_MULTI;
        }
        else if(view_mode == 2){
            *(lisaui_app_setting_wakeup_mode_t *)param = LISAUI_APP_SETTING_WAKEUP_MODE_KEY;
        }
        else{
            *(lisaui_app_setting_wakeup_mode_t *)param = LISAUI_APP_SETTING_WAKEUP_MODE_VOICE_SINGLE;
        }    
    }
    

    return 0;
}

static lisaui_err_t wifi_event_cb(lisaui_wifi_item_t type, lisaui_wifi_op_t op,void *params)
{
    assistant_view_t* view;
    int view_mode = 0;
    int ii;
    
    LISA_LOGI(TAG,"[%s %d]op:%d",__FUNCTION__,__LINE__,op);
    if(view_setting == NULL){
        LISA_LOGW(TAG,"[%s %d]View not initialized");
        return -ENODEV;
    }

    view = CONTAINER_OF(view_setting,assistant_view_t,setting);
    if(op == LISAUI_WIFI_OP_SCAN){
        view->cbs.wifi_hotspot_scan();
    }
    else if(op == LISAUI_WIFI_OP_CONNECT){
        view_wifi_sta_cfg_t sta_cfg;
        wifi_metadata_t* metadata = (wifi_metadata_t*)params;
        
        snprintf(sta_cfg.ssid,sizeof(sta_cfg.ssid),metadata->SSID);
        snprintf(sta_cfg.bssid,sizeof(sta_cfg.bssid),metadata->BSSID);
        LOGI(TAG,"[%s %d]metadata.ssid:%s",__FUNCTION__,__LINE__,metadata->PWD);
        snprintf(sta_cfg.pwd,sizeof(sta_cfg.pwd),metadata->PWD);

        view->cbs.wifi_hotspot_connect(&sta_cfg);
    }
    else if(op == LISAUI_WIFI_OP_DISCONNECT){
        view->cbs.wifi_hotspot_disconnect(NULL);
    }
    else if(op == LISAUI_WIFI_OP_GET_AP_LIST){
        view_wifi_hotspot_list_t *list;
        lisaui_wifi_list_t* lisaui_list = (lisaui_wifi_list_t*)params;

        if((lisaui_list == NULL)||(lisaui_list->count == 0)){
            LISA_LOGE(TAG,"[%s %d]params wrong!",__FUNCTION__,__LINE__);
            return -EINVAL;
        }
        list = exram_malloc(4,sizeof(view_wifi_hotspot_list_t) + sizeof(view_wifi_hotspot_t)*lisaui_list->count);


        view->cbs.wifi_hotspot_list_get(list,lisaui_list->count);

        for(ii=0;ii<list->count && ii < lisaui_list->count;ii++){
            snprintf(lisaui_list->list[ii].BSSID,sizeof(lisaui_list->list[ii].BSSID),"%s",list->hotspots[ii].bssid);
            snprintf(lisaui_list->list[ii].SSID,sizeof(lisaui_list->list[ii].SSID),"%s",list->hotspots[ii].ssid);
            snprintf(lisaui_list->list[ii].PWD,sizeof(lisaui_list->list[ii].PWD),"%s",list->hotspots[ii].pwd);
            lisaui_list->list[ii].rssi = list->hotspots[ii].rssi;
            if(list->hotspots[ii].state == VIEW_WIFI_STATE_CONNECTING){
                lisaui_list->list[ii].status = LISAUI_WIFI_STATUS_CONNECTING;
            }
            else if(list->hotspots[ii].state == VIEW_WIFI_STATE_CONNECTED){
                lisaui_list->list[ii].status = LISAUI_WIFI_STATUS_CONNECTED;
            }
            else{
                lisaui_list->list[ii].status = LISAUI_WIFI_STATUS_DISCONNECT;
            }
            LISA_LOGI(TAG,"[%s %d]Get wifi list index:%d,ssid:%s,status:%d",__FUNCTION__,__LINE__,ii,lisaui_list->list[ii].SSID,lisaui_list->list[ii].status);
        }
        lisaui_list->count = list->count;
        
                    

        exram_free(lisaui_list);

    }
    
    return 0;
}
#endif
